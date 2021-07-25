#include <stdio.h>
#include <curl/curl.h>
#include <string>
#include <filesystem>
#include <iostream>
#include <map>
#include <cmath>
#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <stdexcept>
#include <tuple>

#include "App.h"
#include "Image.h"

namespace fs = std::filesystem;

// Useful Links and Resources
//
// https://operations.osmfoundation.org/policies/tiles/
// https://wiki.openstreetmap.org/wiki/Tile_servers
// https://wiki.openstreetmap.org/wiki/Slippy_map_tilenames

#define TILE_SERVER   "https://a.tile.openstreetmap.org/"
#define TILE_SIZE     256
#define MAX_ZOOM      19
#define MAX_THREADS   12
#define USER_AGENT    "ImMaps/0.1"

struct TileCoord {
    int z; // zoom    [0......20]
    int x; // x index [0...z^2-1]
    int y; // y index [0...z^2-1]
    inline std::string subdir() const { return std::to_string(z) + "/" + std::to_string(x) + "/"; }
    inline std::string dir() const { return "tiles/" + subdir(); }
    inline std::string file() const { return std::to_string(y) + ".png"; }
    inline std::string path() const { return dir() + file(); }
    inline std::string url() const { return TILE_SERVER + subdir() + file(); }
    inline std::string label() const { return subdir() + std::to_string(y); }
    std::tuple<ImPlotPoint,ImPlotPoint> getBounds() {
        double n = std::pow(2,z);
        double t = 1.0 / n;
        return { 
                   { x*t     , (1+y)*t } , 
                   { (1+x)*t , (y)*t   } 
               };
    }
};

bool operator<(const TileCoord& l, const TileCoord& r ) { 
    if ( l.z < r.z )  return true;
    if ( l.z > r.z )  return false;
    if ( l.x < r.x )  return true;
    if ( l.x > r.x )  return false;
    if ( l.y < r.y )  return true;
    if ( l.y > r.y )  return false;
                      return false;
}

enum TileState : int {
    Unavailable = 0,
    Loaded,
    Downloading,
    OnDisk
};

typedef Image TileImage;

struct Tile {
    Tile() : state(TileState::Unavailable) {  }
    Tile(TileState s) : state(s) { }
    TileState state;
    TileImage image;
};

size_t curl_write_cb(void *ptr, size_t size, size_t nmemb, void *userdata) {
    FILE *stream = (FILE *)userdata;
    if (!stream) {
        printf("No stream\n");
        return 0;
    }
    size_t written = fwrite((FILE *)ptr, size, nmemb, stream);
    return written;
}

class TileManager {
public:

    TileManager()  : m_stop(false)
    {
        start_workers();
    }
 
    inline ~TileManager() {
        {
            std::unique_lock<std::mutex> lock(m_queue_mutex);
            m_stop = true;
        }
        m_condition.notify_all();
        for(std::thread &worker: m_workers)
            worker.join();
    }

    const std::vector<std::pair<TileCoord, std::shared_ptr<Tile>>>& get_region(ImPlotLimits view, ImVec2 pixels) {
        double min_x = std::clamp(view.X.Min, 0.0, 1.0);
        double min_y = std::clamp(view.Y.Min, 0.0, 1.0);
        double size_x = std::clamp(view.X.Size(),0.0,1.0);
        double size_y = std::clamp(view.Y.Size(),0.0,1.0);

        double pix_occupied_x = (pixels.x / view.X.Size()) * size_x;
        double pix_occupied_y = (pixels.y / view.Y.Size()) * size_y;
        double units_per_tile_x = view.X.Size() * (TILE_SIZE / pix_occupied_x);
        double units_per_tile_y = view.Y.Size() * (TILE_SIZE / pix_occupied_y);

        int z    = 0;
        double r = 1.0 / pow(2,z);
        while (r > units_per_tile_x && r > units_per_tile_y && z < MAX_ZOOM) 
            r = 1.0 / pow(2,++z);
        
        m_region.clear();
        if (!add_region(z, min_x, min_y, size_x, size_y) && z > 0) {
            add_region(--z, min_x, min_y, size_x, size_y); 
            std::reverse(m_region.begin(),m_region.end());
        } 
        return m_region;
    }    

    std::shared_ptr<Tile> try_get_tile(TileCoord coord) {
        std::lock_guard<std::mutex> lock(m_tiles_mutex);
        if (m_tiles.count(coord)) 
            return get_tile(coord);   
        else if (fs::exists(coord.path())) 
            return load_tile(coord);  
        else 
            download_tile(coord);        
        return nullptr;        
    }

    int load_count() const     { return m_loads;     }
    int download_count() const { return m_downloads; }

private:

    bool add_region(int z, double min_x, double min_y, double size_x, double size_y) {
        int k = pow(2,z);
        double r = 1.0 / k;
        int xa = min_x * k; 
        int xb = xa + ceil(size_x / r) + 1; 
        int ya = min_y * k;
        int yb = ya + ceil(size_y / r) + 1;        
        xb = std::clamp(xb,0,k);
        yb = std::clamp(yb,0,k);
        bool covered = true;
        for (int x = xa; x < xb; ++x) {                
            for (int y = ya; y < yb; ++y) {
                TileCoord coord{z,x,y};
                std::shared_ptr<Tile> tile = try_get_tile(coord);
                m_region.push_back({coord,tile});
                if (tile == nullptr || tile->state != TileState::Loaded)    
                    covered = false;
            }
        }
        return covered;
    }

    void download_tile(TileCoord coord) {        
        m_tiles[coord] = std::make_shared<Tile>(Downloading);
        {
            std::unique_lock<std::mutex> lock(m_queue_mutex);
            m_queue.emplace(coord);
        }
        m_condition.notify_one();
    }

    std::shared_ptr<Tile> get_tile(TileCoord coord) {
        if (m_tiles[coord]->state == Loaded)
            return m_tiles[coord];
        else if (m_tiles[coord]->state == OnDisk)
            return load_tile(coord);
        return nullptr; 
    }

    std::shared_ptr<Tile> load_tile(TileCoord coord) {
        if (!m_tiles.count(coord))
            m_tiles[coord] = std::make_shared<Tile>();
        if (m_tiles[coord]->image.LoadFromFile(coord.path().c_str())) {
            m_tiles[coord]->state = TileState::Loaded;
            m_loads++;
            return m_tiles[coord];  
        }
        printf("TileManager[m]: Failed to load tile %s. Attempting redownload.\n", coord.label());
        if (!fs::remove(coord.path()))
            printf("TileManager[m]: Failed to remove %s\n", coord.path().c_str());
        printf("TileManager[m]: Removed %s\n",coord.path().c_str());
        m_tiles.erase(coord);
        return nullptr;
    }

    void start_workers() {
        for(size_t i = 0; i < MAX_THREADS; ++i) {
            m_workers.emplace_back(
                [this, i] {
                    printf("TileManager[%d]: Thread started\n",i);
                    CURL* curl = curl_easy_init();
                    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
                    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
                    curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
                    for(;;)
                    {
                        TileCoord coord;
                        {
                            std::unique_lock<std::mutex> lock(this->m_queue_mutex);
                            this->m_condition.wait(lock,
                                [this]{ return this->m_stop || !this->m_queue.empty(); });
                            if(this->m_stop && this->m_queue.empty()) {
                                curl_easy_cleanup(curl);
                                printf("TileManager[%d]: Thread terminated\n",i);
                                return;
                            }
                            coord = std::move(this->m_queue.front());
                            this->m_queue.pop();
                        }
                        fs::create_directories(coord.dir());
                        FILE *fp = fopen(coord.path().c_str(), "wb");
                        if (!fp) 
                            printf("TileManager[%d]: Failed to create file on the disk!\n",i);                        
                        curl_easy_setopt(curl, CURLOPT_URL, coord.url().c_str());
                        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
                        CURLcode rc = curl_easy_perform(curl);
                        if (rc) 
                            printf("TileManager[%d]: Failed to download: %s\n", i, coord.url().c_str());                        
                        long res_code = 0;
                        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &res_code);
                        if (!((res_code == 200 || res_code == 201) && rc != CURLE_ABORTED_BY_CALLBACK))  
                            printf("TileManager[%d]: Response code: %d\n",i,res_code);                        
                        fclose(fp);
                        this->m_downloads++;
                        {
                            std::lock_guard<std::mutex> lock(m_tiles_mutex);
                            m_tiles[coord]->state = OnDisk;
                        }
                    }
                }
            );
        }
    }

    std::atomic<int> m_loads     = 0;
    std::atomic<int> m_downloads = 0;

    std::map<TileCoord,std::shared_ptr<Tile>> m_tiles;
    std::vector<std::pair<TileCoord, std::shared_ptr<Tile>>> m_region;

    std::vector<std::thread> m_workers;
    std::queue<TileCoord> m_queue;
    std::mutex m_queue_mutex;
    std::mutex m_tiles_mutex;
    std::condition_variable m_condition;
    bool m_stop;

};

struct ImMaps : public App {
    using App::App;

    void update() override {
        static int renders = 0;
        static bool debug = false;
        if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_A)))
            debug = !debug;

        ImGui::SetNextWindowPos({0,0});
        ImGui::SetNextWindowSize(get_window_size());
        ImGui::Begin("Map",0,ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoTitleBar);
        ImPlot::SetNextPlotLimits(0,1,0,1);
        ImPlotAxisFlags ax_flags = ImPlotAxisFlags_NoLabel | ImPlotAxisFlags_NoTickLabels | ImPlotAxisFlags_NoGridLines| ImPlotAxisFlags_Foreground;
        ImPlot::PushStyleColor(ImPlotCol_InlayText,ClearColor);
        ImPlot::PushStyleColor(ImPlotCol_XAxisGrid,ClearColor);
        ImPlot::PushStyleColor(ImPlotCol_YAxisGrid,ClearColor);
        if (ImPlot::BeginPlot("##Map",0,0,ImVec2(-1,-1),ImPlotFlags_Equal,ax_flags,ax_flags|ImPlotAxisFlags_Invert)) {
            auto size = ImPlot::GetPlotSize();
            auto limits = ImPlot::GetPlotLimits();
            auto& region = mngr.get_region(limits,size);
            renders = 0;
            if (debug) {
                float ys[] = {1,1};
                ImPlot::SetNextFillStyle({1,0,0,1},0.5f);
                ImPlot::PlotShaded("##Bounds",ys,2);
            }
            for (auto& pair : region) {
                TileCoord coord            = pair.first;
                std::shared_ptr<Tile> tile = pair.second;
                auto [bmin,bmax] = coord.getBounds();
                if (tile != nullptr) {
                    auto col = debug ? ((coord.x % 2 == 0 && coord.y % 2 != 0) || (coord.x % 2 != 0 && coord.y % 2 == 0))? ImVec4(1,0,1,1) : ImVec4(1,1,0,1) : ImVec4(1,1,1,1);             
                    ImPlot::PlotImage("##Tiles",(void*)(intptr_t)tile->image.Texture,bmin,bmax,{0,0},{1,1},col);
                }
                if (debug) 
                    ImPlot::PlotText(coord.label().c_str(),(bmin.x+bmax.x)/2,(bmin.y+bmax.y)/2);                
                renders++;                
            }
            ImPlot::EndPlot();
        }
        ImPlot::PopStyleColor(3);
        ImGui::End();

        // ImGui::Begin("Metrics");
        // ImGui::Text("%.2f FPS",ImGui::GetIO().Framerate);
        // ImGui::Checkbox("Debug",&debug);
        // ImGui::Text("Downloads: %d", (int)mngr.download_count());
        // ImGui::Text("Loads: %d", (int)mngr.load_count());
        // ImGui::Text("Renders: %d", renders);
        // ImGui::End();
    }

    TileManager mngr;
};

int main(int argc, char **argv)
{
    ImMaps maps(960,540,"ImMaps");
    maps.run();    
}