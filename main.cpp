#include <chrono>
#include <cmath>
#include <format>
#include <fstream>
#include <iostream>
#include <SFML/Graphics.hpp>
#include "SFML/Graphics/CircleShape.hpp"
#include "SFML/Graphics/Color.hpp"
#include "SFML/Graphics/PrimitiveType.hpp"
#include "SFML/Graphics/Rect.hpp"
#include "SFML/Graphics/RenderWindow.hpp"
#include "SFML/Graphics/VertexArray.hpp"
#include "SFML/System/Time.hpp"
#include "SFML/System/Vector2.hpp"
#include "SFML/Window/Event.hpp"
#include "SFML/Window/Mouse.hpp"
#include "SFML/Window/VideoMode.hpp"
#include "imgui-SFML.h"
#include "imgui.h"
#include <chrono>
#include <functional>
#include <string>
#include <vector>
#include "json.hpp"

using namespace sf;
using namespace std;

RenderWindow window(VideoMode({1024, 1024}), "SFML works");
sf::Clock deltaClock;

const float ROTATION_SPEED = 4.f;
float timeScale = 1.f;

void init();
void load();
void resetRot();
void renderUI();

float dt = 1/60.f;
int currMode = 0;
constexpr int MODE_COUNT = 2;
constexpr std::string MODE_LABELS[MODE_COUNT] = { "editMode","fileMode"};
int cameraTarget = -1;
float cameraZoom = 1.f;
int maxFPS = 60;
bool previewMode = false;
int previewFrames = 60;

View camera;

enum TRAILMODE
{
    ALWAYS = -1,
    NONE = 0,
    OTHER = 1
};

struct Pendulum
{
    Pendulum(float size, float speedRatio):size(size), speedRatio(speedRatio), sprite(5.f), line(PrimitiveType::LineStrip, 2){sprite.setOrigin({5, 5});}
    Vector2f getEndPos()
    {
        return pos + Vector2f(cos(rotAngle) * size, sin(rotAngle)*size);
    }
    void render(RenderWindow& window)
    {
        VertexArray trailLine(PrimitiveType::LineStrip, trail.size());
        for(int i = 0; i < trail.size(); i++)
        {
            trailLine[trail.size()-i-1].position = trail[i];
            float colorRatio = i / (float)trail.size();
        }
        window.draw(trailLine);
        sprite.setPosition(pos);
        window.draw(sprite);
        line[0].position = pos;
        sprite.setPosition(getEndPos());
        window.draw(sprite);
        line[1].position = sprite.getPosition();
        window.draw(line);
    }
    void rotate(float& dt)
    {
        if(frameCount++%trailFrameRatio==1)
        {
            if(trail.size() > trailCount && frameCount != ALWAYS)
                trail.erase(trail.begin());
            trail.push_back(getEndPos());
        }
        rotAngle += dt * ROTATION_SPEED * speedRatio;
    }
    int frameCount = 0;
    int trailCount = 0;
    int trailFrameRatio = 2;
    CircleShape sprite;
    VertexArray line;
    Vector2f pos;
    float rotAngle = 0.f;
    float size = 0.f;
    float speedRatio = 0.f;
    vector<Vector2f> trail;
};

vector<Pendulum> pendulums;


int main()
{
    init();
    while(window.isOpen())
    {
        while(optional<sf::Event> e = window.pollEvent())
        {
            if(e->is<Event::Closed>())window.close();
            ImGui::SFML::ProcessEvent(window,*e);
            if(e->is<Event::Resized>())
            {
                Vector2u size = e->getIf<Event::Resized>()->size;
                Vector2f fSize = (Vector2f)size;
                window.setSize(size);
                camera.setCenter(fSize/2.f);
                if(pendulums.size() > 0) pendulums[0].pos = fSize/2.f;
                camera.setSize(fSize);
            }
        }
        if(previewMode)
        {
            for(auto& v : pendulums)
            {
                v.trail.clear();
                v.rotAngle = 0;
            }
            for(int j = 0; j < previewFrames; j++)
            {
                for(int i = 0; i < pendulums.size(); i++)
                {
                    Pendulum& p = pendulums[i];
                    p.rotate(dt);
                    if(i>0)
                        p.pos = pendulums[i-1].getEndPos(); 
                }

            }
        }
        else
            for(int i = 0; i < pendulums.size(); i++)
            {
                Pendulum& p = pendulums[i];
                p.rotate(dt);
                if(i>0)
                    p.pos = pendulums[i-1].getEndPos(); 
            }
        if(cameraTarget != -1 and cameraTarget < pendulums.size())
            camera.setCenter(pendulums[cameraTarget].getEndPos());
        window.setView(camera);
        ImGui::SFML::Update(window, deltaClock.restart());
        window.clear();
        renderUI();
        for(auto& p: pendulums)
            p.render(window);
        ImGui::SFML::Render(window); 
        window.display();
        dt = deltaClock.restart().asSeconds() * timeScale;

    }
}

void load()
{
    std::ifstream file("res/pendulums.json");
    cout << "loading ?" << endl;
    pendulums.clear();
    if(file.good())
    {
        nlohmann::json data = nlohmann::json::parse(file);
        for(auto& v: data["pendulums"])
        {
            pendulums.push_back(Pendulum(v["size"], v["speedRatio"]));
            pendulums[pendulums.size()-1].trailFrameRatio = v["trailFrameRatio"];
            pendulums[pendulums.size()-1].trailCount = v["trailCount"];
        }
        if(pendulums.size() != 0)
            pendulums[0].pos = Vector2f(window.getSize().x / 2.0, window.getSize().y/2.0);
    }
    file.close();
}

void init()
{
    ImGui::SFML::Init(window);
    window.setFramerateLimit(240);
    load();
    dt = 1/60.f;
}

void resetRot()
{
    for(auto& v: pendulums)
    {
        v.rotAngle = 0;
        v.trail.clear();
        v.frameCount = 0;
    }
}


void renderUI()
{
    ImGui::Begin("Settings");
    string fps = format("Simulated FPS:{:.1f}", 1.0/dt);
    ImGui::Text(fps.c_str());
    ImGui::SliderFloat("Time Scale", &timeScale, 0.05, 12);
    if(ImGui::SliderInt("Max FPS:", &maxFPS, 30, 2000))
        window.setFramerateLimit(maxFPS);
    if(ImGui::Button(MODE_LABELS[currMode].c_str()))
    {
        currMode = (currMode+1)%MODE_COUNT;
        if(currMode == 0) load();
    }
    if(currMode == 0)
    {
        if(ImGui::Button("Refresh File"))
            load();
    }
    else
    {
        static float defaultSize = 50;
        static float defaultSpeed = 1;
        float minSize = 1;
        float maxSize = 300;
        float minSpeed = -5;
        float maxSpeed = 5;
        static int defaultFrameCount = 50;
        ImGui::SliderFloat("Default size",&defaultSize, minSize, maxSize, "%.2f");
        ImGui::SliderFloat("Default speed",&defaultSpeed, minSpeed, maxSpeed, "%.2f");
        if(ImGui::Button("Reset"))
            pendulums.clear();
        if(ImGui::Button("Add Pendulum"))
        {
            pendulums.push_back(Pendulum(defaultSize, defaultSpeed));
            if(pendulums.size() == 1) 
                pendulums[0].pos = Vector2f(window.getSize().x/2.0, window.getSize().y/2.0);
        }
        for(int i = 0; i < pendulums.size(); i++)
        {
            Pendulum& p = pendulums[i];
            string pName = format("Pendulum {}\nSize{}:", i, i);
            if(
            ImGui::SliderFloat(pName.c_str(), &pendulums[i].size, minSize, maxSize, "%.2f") ||
            ImGui::SliderFloat(format("Speed{}", i).c_str(), &pendulums[i].speedRatio, minSpeed, maxSpeed, "%.2f") 
            ) resetRot();
            if(ImGui::RadioButton(format("None{}", i).c_str(), p.trailCount == 0))
            {
                p.trailCount = 0;
                p.trail.clear();
            } 
            if(ImGui::RadioButton(format("Always{}", i).c_str(), p.trailCount == ALWAYS))
            {
                p.trailCount = ALWAYS;
                p.trail.clear();
            }
            bool b = p.trailCount >= OTHER;
            if(ImGui::RadioButton(format("Frame{}", i).c_str(), b))
            {
                p.trailCount = defaultFrameCount;
                p.trail.clear();
            }
            if(b)ImGui::InputInt("FrameCount", &p.trailCount);
        }
        static char buff[256] = "default";
        ImGui::InputText("fileName", buff, IM_ARRAYSIZE(buff));
        if(ImGui::Button("Save"))
        {
            cout << buff << endl;
            nlohmann::json data;
            data["pendulums"] = nlohmann::json::array();
            for(int i = 0; i < pendulums.size(); i++)
            {
                Pendulum& p = pendulums[i];
                data["pendulums"][i]["size"] = p.size;
                data["pendulums"][i]["speedRatio"] = p.speedRatio;
                data["pendulums"][i]["trailCount"] = p.trailCount;
                data["pendulums"][i]["trailFrameRatio"] = p.trailFrameRatio;
            }
            ofstream f(buff);
            f << data;
            f.close();
        }
    }

    ImGui::End();
    ImGui::Begin("Camera Controls");
    if(ImGui::SliderInt("TargetIndex:", &cameraTarget, -1, pendulums.size()-1))
        if(cameraTarget == -1)
            camera.setCenter((Vector2f)window.getSize()/2.f);
    if(ImGui::SliderFloat("CameraZoom", &cameraZoom, 0.3, 20.f))camera.setSize((Vector2f)window.getSize() * 1.f/cameraZoom);
    ImGui::Checkbox("Preview Mode", &previewMode);
    if(previewMode)
        ImGui::InputInt("PreviewFrameCount", &previewFrames);
    ImGui::End();
    ImGui::EndFrame();
}