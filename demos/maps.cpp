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
#define MAX_ZOOM      19
#define MAX_THREADS   12
#define USER_AGENT    "ImPlot Maps/0.1"
#define MAX_TILE_SIZE 384 // 50% zoom max

struct TileCoord {

    int z; // zoom    [0......20]
    int x; // x index [0...z^2-1]
    int y; // y index [0...z^2-1]

    inline std::string subdir() const { return std::to_string(z) + "/" + std::to_string(x) + "/"; }
    inline std::string dir() const { return "osm/" + subdir(); }
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

bool operator<(const TileCoord& l, const TileCoord& r )
{ 
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
    Available,
    Downloading
};

typedef Image TileImage;

struct Tile {
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

class TileDownloader {
public:
    
    inline TileDownloader(size_t threads) : stop(false)
    {
        for(size_t i = 0;i<threads;++i) {
            workers.emplace_back(
                [this, i] {
                    printf("TileDownloader[%d]: Thread started\n",i);
                    CURL* curl = curl_easy_init();
                    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
                    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
                    curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
                    for(;;)
                    {
                        TileCoord coord;
                        {
                            std::unique_lock<std::mutex> lock(this->queue_mutex);
                            this->condition.wait(lock,
                                [this]{ return this->stop || !this->queue.empty(); });
                            if(this->stop && this->queue.empty()) {
                                curl_easy_cleanup(curl);
                                printf("TileDownloader[%d]: Thread terminated\n",i);
                                return;
                            }
                            coord = std::move(this->queue.front());
                            this->queue.pop();
                        }
                        fs::create_directories(coord.dir());
                        FILE *fp = fopen(coord.path().c_str(), "wb");
                        if (!fp) 
                            printf("TileDownloader[%d]: Failed to create file on the disk!\n",i);                        
                        curl_easy_setopt(curl, CURLOPT_URL, coord.url().c_str());
                        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
                        CURLcode rc = curl_easy_perform(curl);
                        if (rc) 
                            printf("TileDownloader[%d]: Failed to download: %s\n", i, coord.url().c_str());                        
                        long res_code = 0;
                        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &res_code);
                        if (!((res_code == 200 || res_code == 201) && rc != CURLE_ABORTED_BY_CALLBACK))  
                            printf("TileDownloader[%d]: Response code: %d\n",i,res_code);                        
                        fclose(fp);
                        this->downloads++;
                    }
                }
            );
        }
    }

    inline ~TileDownloader() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for(std::thread &worker: workers)
            worker.join();
    }

    void submit(TileCoord coord) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            if (stop)
                throw std::runtime_error("Download on stop");
            queue.emplace(coord);
        }
        condition.notify_one();
    }

    std::atomic<int> downloads = 0;

private:
    std::vector<std::thread> workers;
    std::queue<TileCoord> queue;
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};


class TileManager {
public:

    TileManager() : m_downloader(MAX_THREADS) { }  

    std::shared_ptr<Tile> get_tile(TileCoord coord) {
        if (m_tiles.count(coord)) {
            if (m_tiles[coord]->state == TileState::Available)
                return m_tiles[coord];    
            if (m_tiles[coord]->state == TileState::Downloading && fs::exists(coord.path()))    
                return load_tile(coord);  
            else
                return nullptr;             
        }
        else if (fs::exists(coord.path())) {
            m_tiles[coord] = std::make_shared<Tile>();
            return load_tile(coord);
        }   
        else {
            m_tiles[coord] = std::make_shared<Tile>();
            m_tiles[coord]->state = TileState::Downloading;
            m_downloader.submit(coord);
            return nullptr;
        }
    }

    const std::vector<std::shared_ptr<Tile>>& get_region(ImPlotLimits limits, ImVec2 size) {
        double range_x = std::clamp(limits.X.Size(),0.0,1.0);
        double range_y = std::clamp(limits.Y.Size(),0.0,1.0);
        double pix_occupied_x = (size.x / limits.X.Size()) * range_x;
        double pix_occupied_y = (size.y / limits.Y.Size()) * range_y;
        double units_per_tile_x = limits.X.Size() / (pix_occupied_x / MAX_TILE_SIZE);
        double units_per_tile_y = limits.Y.Size() / (pix_occupied_y / MAX_TILE_SIZE);

        int z    = 0;
        int k    = pow(2,z);
        double r = 1.0 / k;
        while (r > units_per_tile_x && r > units_per_tile_y && z < MAX_ZOOM) {
            k = pow(2,++z);
            r = 1.0 / k;
        }

        double xmin = std::clamp(limits.X.Min, 0.0, 1.0);
        double ymin = std::clamp(limits.Y.Min, 0.0, 1.0);

        int xa = xmin * k; 
        int xb = xa + ceil(range_x / r) + 1; 
        int ya = ymin * k;
        int yb = ya + ceil(range_y / r) + 1;  
        
        xb = std::clamp(xb,0,k);
        yb = std::clamp(yb,0,k);

        m_region.clear();
        for (int x = xa; x < xb; ++x) {                
            for (int y = ya; y < yb; ++y) {
                TileCoord coord{z,x,y};
                std::shared_ptr<Tile> tile = get_tile(coord);
                m_region.push_back(tile);
            }
        }
    }
    

    int load_count() const { return m_loads; }
    int download_count() const { return m_downloader.downloads; }

private:

    std::shared_ptr<Tile> load_tile(TileCoord tile) {
        if (m_tiles[tile]->image.LoadFromFile(tile.path().c_str())) {
            m_tiles[tile]->state = TileState::Available;
            m_loads++;
            return m_tiles[tile];  
        }
        return nullptr;
    }

    int m_loads = 0;
    std::map<TileCoord,std::shared_ptr<Tile>> m_tiles;
    std::vector<std::shared_ptr<Tile>> m_region;
    std::mutex m_mutex;
    TileDownloader m_downloader;
};


struct ImPlotMaps : public App {
    using App::App;

    void update() override {
        static int renders = 0;
        static bool debug = false;

        ImGui::Begin("Map");
        ImPlot::SetNextPlotLimits(0,1,0,1);
        ImPlotAxisFlags ax_flags = ImPlotAxisFlags_NoDecorations;
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
            for (auto& tile : region) {
                auto [bmin,bmax] = coord.getBounds();
                if (tile != nullptr) {
                    auto col = debug ? ((x % 2 == 0 && y % 2 != 0) || (x % 2 != 0 && y % 2 == 0))? ImVec4(1,0,1,1) : ImVec4(1,1,0,1) : ImVec4(1,1,1,1);             
                    ImPlot::PlotImage("##Tiles",(void*)(intptr_t)tile->image.Texture,bmin,bmax,{0,0},{1,1},col);
                }
                if (debug) {
                    ImPlot::PushStyleColor(ImPlotCol_InlayText,IM_COL32_BLACK);
                    ImPlot::PlotText(coord.label().c_str(),(bmin.x+bmax.x)/2,(bmin.y+bmax.y)/2);
                    ImPlot::PopStyleColor();
                }
                renders++;                
            }
            ImPlot::EndPlot();
        }
        ImGui::End();

        ImGui::Begin("Metrics");
        ImGui::Text("%.2f FPS",ImGui::GetIO().Framerate);
        ImGui::Checkbox("Debug",&debug);

        ImGui::Text("Downloads: %d", (int)mngr.download_count());
        ImGui::Text("Loads: %d", (int)mngr.load_count());
        ImGui::Text("Renders: %d", renders);

        ImGui::End();
    }

    TileManager mngr;
};

int main(int argc, char **argv)
{
    ImPlotMaps maps(1280,720,"ImPlot Maps");
    maps.run();
}