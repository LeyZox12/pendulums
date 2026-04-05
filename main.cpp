#include <chrono>
#include <cmath>
#include <filesystem>
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
#include "SFML/Window/Keyboard.hpp"
#include "SFML/Window/Mouse.hpp"
#include "SFML/Window/VideoMode.hpp"
#include "imgui-SFML.h"
#include "imgui.h"
#include <chrono>
#include <functional>
#include <string>
#include <vector>
#include "json.hpp"
#include <algorithm>

using namespace sf;
using namespace std;

RenderWindow window(VideoMode({1024, 1024}), "SFML works");
sf::Clock deltaClock;

float backgroundColor[3] = {0, 0, 0};
float penColor[3] = {255, 255, 255};

const float ROTATION_SPEED = 4.f;
float timeScale = 1.f;

void init();
void load();
void resetRot();
void renderUI();
void cancel();
Color hsv(int hue, float sat, float val);

float dt = 1/60.f;
int currMode = 0;
constexpr int MODE_COUNT = 2;
constexpr std::string MODE_LABELS[MODE_COUNT] = { "editMode","fileMode"};
int cameraTarget = -1;
float cameraZoom = 1.f;
int maxFPS = 60;
bool previewMode = false;
int previewFrames = 60;
float minZoom = 0.1f;
const char* fileName = "res/default.json";
View camera;

bool rainbow = false;

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
    void renderTrail(RenderWindow& window)
    {
        VertexArray trailLine(PrimitiveType::LineStrip, trail.size());
        for(int i = 0; i < trail.size(); i++)
        {
            if(rainbow) trailLine[trail.size()-1-i].color = hsv(i%360, 1, 1);
            else trailLine[trail.size()-i-1].color = Color(penColor[0] * 255, penColor[1] * 255, penColor[2] * 255);
            trailLine[trail.size()-i-1].position = trail[i];
            float colorRatio = i / (float)trail.size();
        }
        window.draw(trailLine);
    }

    void renderPendulum(RenderWindow& window)
    {
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

vector<pair<int, Pendulum>> deleted;
vector<int> restored;

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
            if(e->is<Event::KeyPressed>())
            {
                Keyboard::Key k = e->getIf<Event::KeyPressed>() -> code;
                if(k == Keyboard::Key::Z && Keyboard::isKeyPressed(Keyboard::Key::LControl))
                    cancel();
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
        window.clear(Color(backgroundColor[0] * 255, backgroundColor[1] * 255, backgroundColor[2] * 255));
        renderUI();
        for(auto& p: pendulums)
            p.renderTrail(window);
        for(auto& p: pendulums)
            p.renderPendulum(window);
        ImGui::SFML::Render(window); 
        window.display();
        dt = deltaClock.restart().asSeconds() * timeScale;

    }
}

void load()
{
    std::ifstream file(fileName);
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

    ImGui::ColorPicker3("BackgroundColor", backgroundColor);
    ImGui::ColorPicker3("PenColor", penColor);
    ImGui::Checkbox("RainbowTrail", &rainbow);
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
        vector<const char*> fileNames;
        for(auto& f: filesystem::directory_iterator("res/"))
            fileNames.push_back(f.path().string().c_str());
        static int itemIndex = 0;
        if(ImGui::Combo("MyCombo", &itemIndex, fileNames.data(), fileNames.size()))
        {
            fileName = fileNames[itemIndex];
            load();
        }
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
            if(ImGui::Button(format("Delete{}", i).c_str()))
            {
                deleted.push_back(make_pair(i, pendulums[i]));
                pendulums.erase(pendulums.begin() + i);
                resetRot();
                for(auto& d: deleted)
                {
                    if(d.first >= i) d.first -= 1;
                }
            }
        }
        string before = "res/";
        const string after = ".json";
        static char buff[256] = "default";
        ImGui::InputText("fileName", buff, IM_ARRAYSIZE(buff));
        if(ImGui::Button("Save"))
        {
            before.append(buff);
            before.append(after);
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
            ofstream f(before);
            f << data;
            f.close();
        }
    }

    ImGui::End();
    ImGui::Begin("Camera Controls");
    if(ImGui::SliderInt("TargetIndex:", &cameraTarget, -1, pendulums.size()-1))
        if(cameraTarget == -1)
            camera.setCenter((Vector2f)window.getSize()/2.f);
    if(ImGui::DragFloat("CameraZoom", &cameraZoom, 0.1, 0.1))
    {
        cameraZoom = max(0.1f, cameraZoom);    
        camera.setSize((Vector2f)window.getSize() * 1.f/cameraZoom);
    }
    ImGui::Checkbox("Preview Mode", &previewMode);
    if(previewMode)
        ImGui::InputInt("PreviewFrameCount", &previewFrames);
    ImGui::End();
    ImGui::EndFrame();
}

sf::Color hsv(int hue, float sat, float val)
{
  hue %= 360;
  while(hue<0) hue += 360;

  if(sat<0.f) sat = 0.f;
  if(sat>1.f) sat = 1.f;

  if(val<0.f) val = 0.f;
  if(val>1.f) val = 1.f;

  int h = hue/60;
  float f = float(hue)/60-h;
  float p = val*(1.f-sat);
  float q = val*(1.f-sat*f);
  float t = val*(1.f-sat*(1-f));

  switch(h)
  {
    default:
    case 0:
    case 6: return sf::Color(val*255, t*255, p*255);
    case 1: return sf::Color(q*255, val*255, p*255);
    case 2: return sf::Color(p*255, val*255, t*255);
    case 3: return sf::Color(p*255, q*255, val*255);
    case 4: return sf::Color(t*255, p*255, val*255);
    case 5: return sf::Color(val*255, p*255, q*255);
  }
}

void cancel()
{
    if(deleted.size() != 0)
    {
        pair<int, Pendulum> &p = deleted[deleted.size()-1];
        pendulums.insert(pendulums.begin() + p.first, p.second);
        restored.push_back(p.first);
        deleted.erase(deleted.end());
    }
}