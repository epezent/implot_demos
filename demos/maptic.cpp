#include "App.h"
#include <json.hpp>
#include <fstream>
#include <iomanip>
#include <map>
#include <imnodes.h>

using json = nlohmann::json;
using std::vector;
using std::map;
using std::string;

struct ScalarMapping {
    float min, max;
};

struct VectorMapping {
    float min, max, radialScale, normalScale, bimanualScale;
    bool invert, bimanual;
};

struct ScalarConfig {
    struct Data {
       bool enabled;
       ScalarMapping mapping; 
    };
    map<string,Data> uniforms;
};

struct VectorConfig {
    struct Data {
       bool enabled;
       VectorMapping mapping; 
    };
    map<string,Data> uniforms;
    map<string,Data> spatials;
};

struct InteractionConfig {
    map<string,ScalarConfig> scalars;
    map<string,VectorConfig> vectors;
};

struct WristbandConfig {
    map<string, InteractionConfig> interactions;
};

struct WristbandHeader {
    string name;
    vector<string> uniforms;
    vector<string> spatials;
};

struct InteractionHeader {
    string name;
    vector<string> scalars;
    vector<string> vectors;
};

struct MapticConfig {
    map<string,WristbandConfig> configurations;
    vector<WristbandHeader> wristbands;
    vector<InteractionHeader> interactions;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ScalarMapping, min, max);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(VectorMapping, min, max, radialScale, normalScale, bimanualScale);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ScalarConfig::Data, enabled, mapping);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(VectorConfig::Data, enabled, mapping);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ScalarConfig, uniforms);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(VectorConfig, uniforms, spatials);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(InteractionConfig, scalars, vectors);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(WristbandConfig, interactions);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(WristbandHeader, name, uniforms, spatials);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(InteractionHeader, name, scalars, vectors);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(MapticConfig, configurations, wristbands, interactions);

struct Maptic : App {
    using App::App;
    ~Maptic() { ImNodes::DestroyContext(); }
    void Start() override { ImNodes::CreateContext(); LoadConfig("test_config.json"); }
    void Update() override {

        ImGui::SetNextWindowPos(ImVec2(0,0),ImGuiCond_Always);
        ImGui::SetNextWindowSize(GetWindowSize(), ImGuiCond_Always);
        ImGui::Begin("Maptic",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_MenuBar|ImGuiWindowFlags_NoResize);
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Open File...")) {
                    string path;
                    if (OpenDialog(path,{{"JSON File","json"}}) == DialogOkay) {
                        LoadConfig(path);
                    }                    
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        static int wrist_idx = 0;
        static int inter_idx = 0;

        const int num_wrist = m_config.wristbands.size();
        const int num_inter = m_config.interactions.size();

        if (ImGui::BeginCombo("Wristband", num_wrist > 0 ? m_config.wristbands[wrist_idx].name.c_str() : "none")) {
            for (int i = 0; i < num_wrist; ++i) {
                if (ImGui::Selectable(m_config.wristbands[i].name.c_str(), i==wrist_idx))
                    wrist_idx = i;
            }
            ImGui::EndCombo();
        }
        
        if (ImGui::BeginCombo("Interaction", num_inter > 0 ? m_config.interactions[inter_idx].name.c_str() : "none")) {
            
            for (int i = 0; i < num_inter; ++i) {
                if (ImGui::Selectable(m_config.interactions[i].name.c_str(), i==inter_idx))
                    inter_idx = i;
            }
            ImGui::EndCombo();
        }

        ImGui::Separator();

        const bool loaded = num_wrist > 0 && num_inter > 0;

        if (loaded) {


            auto& wrist_hdr = m_config.wristbands[wrist_idx];
            auto& inter_hdr = m_config.interactions[inter_idx];

            auto& wrist_str = wrist_hdr.name;
            auto& inter_str = inter_hdr.name;

            auto& subconfig = m_config.configurations.at(wrist_str).interactions.at(inter_str);

            const int num_unif = wrist_hdr.uniforms.size();
            const int num_spat = wrist_hdr.spatials.size();
            const int num_scal = inter_hdr.scalars.size();
            const int num_vect = inter_hdr.vectors.size();

            const int num_cols = 1 + wrist_hdr.uniforms.size() + wrist_hdr.spatials.size();
            const int num_rows = inter_hdr.scalars.size() + inter_hdr.vectors.size();

            if (ImGui::BeginTable("##MapingMatrix",num_cols)) {
                
                ImGui::TableSetupColumn("");
                static char buff[128];
                for (auto& u : wrist_hdr.uniforms) {
                    sprintf(buff, "%s " ICON_FA_CIRCLE, u.c_str());
                    ImGui::TableSetupColumn(buff);
                }
                for (auto& s : wrist_hdr.spatials) {
                    sprintf(buff, "%s " ICON_FA_DOT_CIRCLE, s.c_str());
                    ImGui::TableSetupColumn(buff);
                }

                ImGui::TableHeadersRow();
                
                for (int i = 0; i < num_scal; ++i) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    auto& sca_str = inter_hdr.scalars[i];
                    auto& sca_cfg = subconfig.scalars.at(sca_str);

                    ImGui::Text("%s " ICON_FA_LOCATION_ARROW, sca_str.c_str());

                    ImGui::PushID("ScalarUniforms");
                    for (int j = 0; j < num_unif; ++j) {
                        ImGui::TableNextColumn();
                        auto& uni_str =  wrist_hdr.uniforms[j];
                        ImGui::PushID(j);
                        ImGui::Checkbox("##Enabled",&sca_cfg.uniforms.at(uni_str).enabled);
                        ImGui::PopID();                      
                    }
                    ImGui::PopID();
                }

                for (int i = 0; i < num_vect; ++i) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    auto& vec_str = inter_hdr.vectors[i];
                    auto& vec_cfg = subconfig.vectors.at(vec_str);

                    ImGui::Text("%s " ICON_FA_LOCATION_ARROW, vec_str.c_str());

                    ImGui::PushID("VectorUniforms");
                    for (int j = 0; j < num_unif; ++j) {
                        ImGui::TableNextColumn();
                        auto& uni_str =  wrist_hdr.uniforms[j];
                        ImGui::PushID(j);
                        ImGui::Checkbox("##Enabled",&vec_cfg.uniforms.at(uni_str).enabled);
                        ImGui::PopID();                      
                    }
                    ImGui::PopID();

                    ImGui::PushID("VectorSpatials");
                    for (int j = 0; j < num_spat; ++j) {
                        ImGui::TableNextColumn();
                        auto& spa_str = wrist_hdr.spatials[j];
                        ImGui::PushID(j);
                        ImGui::Checkbox("##Enabled",&vec_cfg.spatials.at(spa_str).enabled);    
                        ImGui::PopID();                                       
                    }
                    ImGui::PopID();
                }


                ImGui::EndTable();
            }            
        }

        ImGui::End();
    }

    void LoadConfig(string path) {
        std::ifstream file(path);
        if (file.is_open()) {
            json j;
            file >> j;
            try {
                j.get_to(m_config);
                std::cout << "Loaded configuration file: " << path << std::endl;
            }
            catch (...) {
                std::cout << "Failed to parse configuration file!" << std::endl;
            }
        }
        else {
            std::cout << "Failed to open configuration file!" << std::endl;
        }
    }

    void SaveConfig(string path) {

    }

    MapticConfig m_config;
};

int main(int argc, char const *argv[])
{
    Maptic app("Maptic", 640, 480, argc, argv);
    app.Run();
}