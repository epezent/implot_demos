// Demo:   maps.cpp
// Author: Evan Pezent (evanpezent.com)
// Date:   7/25/2021

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

#define TILE_SERVER   "https://a.tile.openstreetmap.org/" // the tile map server url
#define TILE_SIZE     256                                 // the expected size of tiles in pixels, e.g. 256x256px
#define MAX_ZOOM      19                                  // the maximum zoom level provided by the server
#define MAX_THREADS   2                                   // the maximum threads to use for downloading tiles (OSC stricly forbids more than 2)
#define USER_AGENT    "ImMaps/0.1"                        // change this to represent your own app if you extend this code

#define PI 3.14159265359

int long2tilex(double lon, int z) { 
	return (int)(floor((lon + 180.0) / 360.0 * (1 << z))); 
}

int lat2tiley(double lat, int z) { 
    double latrad = lat * PI/180.0;
	return (int)(floor((1.0 - asinh(tan(latrad)) / PI) / 2.0 * (1 << z))); 
}

double tilex2long(int x, int z) {
	return x / (double)(1 << z) * 360.0 - 180;
}

double tiley2lat(int y, int z) {
	double n = PI - 2.0 * PI * y / (double)(1 << z);
	return 180.0 / PI * atan(0.5 * (exp(n) - exp(-n)));
}

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
    std::tuple<ImPlotPoint,ImPlotPoint> bounds() const {
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
    Unavailable = 0, // tile not available
    Loaded,          // tile has been loaded into  memory
    Downloading,     // tile is downloading from server
    OnDisk           // tile is saved to disk, but not loaded into memory
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

    TileManager() {
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
        if (!append_region(z, min_x, min_y, size_x, size_y) && z > 0) {
            append_region(--z, min_x, min_y, size_x, size_y); 
            std::reverse(m_region.begin(),m_region.end());
        } 
        return m_region;
    }    

    std::shared_ptr<Tile> request_tile(TileCoord coord) {
        std::lock_guard<std::mutex> lock(m_tiles_mutex);
        if (m_tiles.count(coord)) 
            return get_tile(coord);   
        else if (fs::exists(coord.path())) 
            return load_tile(coord);  
        else 
            download_tile(coord);        
        return nullptr;        
    }

    int tiles_loaded() const     { return m_loads;     }
    int tiles_downloaded() const { return m_downloads; }
    int tiles_cached() const     { return m_loads - m_downloads; }
    int tiles_failed() const     { return m_fails; }
    int threads_working() const  { return m_working; }

private:

    bool append_region(int z, double min_x, double min_y, double size_x, double size_y) {
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
                std::shared_ptr<Tile> tile = request_tile(coord);
                m_region.push_back({coord,tile});
                if (tile == nullptr || tile->state != TileState::Loaded)    
                    covered = false;
            }
        }
        return covered;
    }

    void download_tile(TileCoord coord) { 
        auto dir = coord.dir();
        fs::create_directories(dir);
        if (fs::exists(dir)) {       
            m_tiles[coord] = std::make_shared<Tile>(Downloading);
            {
                std::unique_lock<std::mutex> lock(m_queue_mutex);
                m_queue.emplace(coord);
            }
            m_condition.notify_one();
        }
    }

    std::shared_ptr<Tile> get_tile(TileCoord coord) {
        if (m_tiles[coord]->state == Loaded)
            return m_tiles[coord];
        else if (m_tiles[coord]->state == OnDisk)
            return load_tile(coord);
        return nullptr; 
    }

    std::shared_ptr<Tile> load_tile(TileCoord coord) {
        auto path = coord.path();
        if (!m_tiles.count(coord))
            m_tiles[coord] = std::make_shared<Tile>();
        if (m_tiles[coord]->image.LoadFromFile(path.c_str())) {
            m_tiles[coord]->state = TileState::Loaded;
            m_loads++;
            return m_tiles[coord];  
        }
        m_fails++;
        printf("TileManager[00]: Failed to load \"%s\"\n", path.c_str());
        if (!fs::remove(path))
            printf("TileManager[00]: Failed to remove \"%s\"\n", path.c_str());
        printf("TileManager[00]: Removed \"%s\"\n",path.c_str());
        m_tiles.erase(coord);
        return nullptr;
    }

    void start_workers() {
        for(int thrd = 1; thrd < MAX_THREADS+1; ++thrd) {
            m_workers.emplace_back(
                [this, thrd] {
                    printf("TileManager[%02d]: Thread started\n",thrd);
                    CURL* curl = curl_easy_init();
                    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
                    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
                    curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
                    for(;;)
                    {
                        TileCoord coord;
                        {
                            std::unique_lock<std::mutex> lock(m_queue_mutex);
                            m_condition.wait(lock,
                                [this]{ return m_stop || !m_queue.empty(); });
                            if(m_stop && m_queue.empty()) {
                                curl_easy_cleanup(curl);
                                printf("TileManager[%02d]: Thread terminated\n",thrd);
                                return;
                            }
                            coord = std::move(m_queue.front());
                            m_queue.pop();
                        }
                        m_working++;
                        bool success = true;
                        auto dir = coord.dir();
                        auto path = coord.path();
                        auto url = coord.url();
                        FILE *fp = fopen(coord.path().c_str(), "wb");
                        if (fp) {
                            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                            curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
                            CURLcode cc = curl_easy_perform(curl);
                            fclose(fp);
                            if (cc != CURLE_OK) {
                                printf("TileManager[%02d]: Failed to download: \"%s\"\n", thrd, url.c_str());  
                                long rc = 0;
                                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &rc);
                                if (!((rc == 200 || rc == 201) && rc != CURLE_ABORTED_BY_CALLBACK))  
                                    printf("TileManager[%02d]: Response code: %d\n",thrd,rc);      
                                fs::remove(coord.path());
                                success = false;
                            }      
                        }
                        else {
                            printf("TileManager[%02d]: Failed to open or create file \"%s\"\n",thrd, path.c_str());   
                            success = false;                    
                        }
                        if (success) {
                            m_downloads++;
                            std::lock_guard<std::mutex> lock(m_tiles_mutex);
                            m_tiles[coord]->state = OnDisk;
                        }
                        else {
                            m_fails++;
                            std::lock_guard<std::mutex> lock(m_tiles_mutex);
                            m_tiles.erase(coord);
                        }
                        m_working--;
                    }
                }
            );
        }
    }

    std::atomic<int> m_loads     = 0;
    std::atomic<int> m_downloads = 0;
    std::atomic<int> m_fails     = 0;
    std::atomic<int> m_working   = 0;
    std::map<TileCoord,std::shared_ptr<Tile>> m_tiles;
    std::mutex m_tiles_mutex;
    std::vector<std::pair<TileCoord, std::shared_ptr<Tile>>> m_region;
    std::vector<std::thread> m_workers;
    std::queue<TileCoord> m_queue;
    std::mutex m_queue_mutex;
    std::condition_variable m_condition;
    bool m_stop = false;
};

struct ImMaps : public App {
    using App::App;

    void Update() override {
        static int renders = 0;
        static bool debug = false;
        if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_A)))
            debug = !debug;

        ImGui::SetNextWindowPos({0,0});
        ImGui::SetNextWindowSize(GetWindowSize());
        ImGui::Begin("Map",0,ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoTitleBar);
        if (debug) {
            int wk = mngr.threads_working();
            int dl = mngr.tiles_downloaded();
            int ld = mngr.tiles_loaded();
            int ca = mngr.tiles_cached();
            int fa = mngr.tiles_failed();
            ImGui::Text("FPS: %.2f    Working: %d    Downloads: %d    Loads: %d    Caches: %d    Fails: %d    Renders: %d", ImGui::GetIO().Framerate, wk, dl, ld, ca, fa, renders);
        }

        ImPlot::SetNextPlotLimits(0,1,0,1);
        ImPlotAxisFlags ax_flags = ImPlotAxisFlags_NoLabel | ImPlotAxisFlags_NoTickLabels | ImPlotAxisFlags_NoGridLines| ImPlotAxisFlags_Foreground;
        if (ImPlot::BeginPlot("##Map",0,0,ImVec2(-1,-1),ImPlotFlags_Equal|ImPlotFlags_NoMousePos,ax_flags,ax_flags|ImPlotAxisFlags_Invert)) {
            auto pos  = ImPlot::GetPlotPos();
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
                auto [bmin,bmax] = coord.bounds();
                if (tile != nullptr) {
                    auto col = debug ? ((coord.x % 2 == 0 && coord.y % 2 != 0) || (coord.x % 2 != 0 && coord.y % 2 == 0))? ImVec4(1,0,1,1) : ImVec4(1,1,0,1) : ImVec4(1,1,1,1);             
                    ImPlot::PlotImage("##Tiles",(void*)(intptr_t)tile->image.ID,bmin,bmax,{0,0},{1,1},col);
                }
                if (debug) 
                    ImPlot::PlotText(coord.label().c_str(),(bmin.x+bmax.x)/2,(bmin.y+bmax.y)/2);                
                renders++;                
            }
            ImPlot::PushPlotClipRect();
            static const char* label = ICON_FA_COPYRIGHT " OpenStreetMap Contributors";
            auto label_size = ImGui::CalcTextSize(label);
            auto label_off  = ImPlot::GetStyle().MousePosPadding;
            ImPlot::GetPlotDrawList()->AddText({pos.x + label_off.x, pos.y+size.y-label_size.y-label_off.y},IM_COL32_BLACK,label);
            ImPlot::PopPlotClipRect();            
            ImPlot::EndPlot();
        }
        ImGui::End();
    }

    TileManager mngr;
};

int main(int argc, char const *argv[])
{
    ImMaps app("ImMaps",960,540,argc,argv);
    app.Run();    
}