/*
    Copyright 2011, Ilya Popov <ilia_popov@rambler.ru>

    This file is part of osmview.

    osmview is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    osmview is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with osmview.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "tilecache.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

#include "tilecacheitem.hpp"

#include <SDL2pp/Font.hh>
#include <SDL2pp/Exception.hh>

osmview::TileCache::TileCache(const std::string &tile_dir,
                              const std::string &url_base,
                              SDL2pp::Renderer &renderer,
                              size_t max_size)
    :
    tile_dir_(tile_dir),
    url_base_(url_base),
    fetcher_(std::thread::hardware_concurrency()),
    downloader_(16),
    renderer_(renderer),
    loading_texture_(generate_loading_texture()),
    max_size_(max_size)
{}

osmview::TileCache::~TileCache()
{}

SDL2pp::Texture & osmview::TileCache::get_texture(int level, int i, int j,
                                                  size_t timestamp)
{
    if (level < 0 || i < 0 || j < 0 || i >= (1<<level) || j >= (1<<level))
    {
        std::ostringstream oss;
        oss << "Requested wrong tile coordinates: "<< level << ' ' << i << ' ' << j;
        throw std::logic_error(oss.str());
    }

    key_t key = make_key(level, i, j);
    
    map_t::iterator p = cache_.find(key);

    if (p == cache_.end())
    {
        //if (size() >= max_size_)
        //{
        //    gc();
        //}

        std::string file_name = make_file_name(level, i, j);
        std::string url = make_url(level, i, j);
        
        std::unique_ptr<TileCacheItem> tci(new TileCacheItem(this, key, file_name, url));
        p = cache_.emplace(key, std::move(tci)).first;
    }

    auto & texture = p->second->get_texture(renderer_, timestamp);

    if (texture)
        return *texture;
    return loading_texture_;
}

SDL2pp::Texture osmview::TileCache::generate_loading_texture()
{
    SDL2pp::Surface tile(0, 256, 256, 32, 0, 0, 0, 0);
    tile.FillRect(SDL2pp::NullOpt, 0x00000000);

    try
    {
        SDL2pp::Font font("data/ClearSans-Medium.ttf", 24);
        auto text = font.RenderText_Blended("Loading...", {128, 128, 128, 0});
        text.Blit(SDL2pp::NullOpt, tile, {(tile.GetSize() - text.GetSize())/ 2, text.GetSize()});
    }
    catch (SDL2pp::Exception &e)
    {
        std::cerr << "Warning: Cannot render text: " << e.what() << std::endl;
    }

    return SDL2pp::Texture(renderer_, tile);
}

void osmview::TileCache::gc()
{
    // Not thread safe: can delete tiles while downloading or loading!!!
    throw std::logic_error("Garbage collection not implemented correctly yet");

    size_t before = cache_.size();

    std::vector<size_t> timestamps;
    timestamps.reserve(cache_.size());

    for (const auto & item: cache_)
    {
        timestamps.push_back(item.second->access_timestamp());
    }

    // Will remove the oldest quarter of tiles
    auto threshold_pos = timestamps.begin() + timestamps.size() / 4;
    std::nth_element(timestamps.begin(), timestamps.end(), threshold_pos);
    size_t threshold = *threshold_pos;

    auto i = cache_.begin();
    while (i != cache_.end())
    {
        if (i->second->access_timestamp() < threshold)
        {
            i = cache_.erase(i);
        }
        else
        {
            ++i;
        }
    }

    std::cout << "Garbage collected: " << before << " -> " << cache_.size() << std::endl;
}

osmview::TileCache::key_t osmview::TileCache::make_key(int level, int i, int j)
{
    std::ostringstream ss;
    ss << level << '/' << i << '/' << j;

    return ss.str();
}

std::string osmview::TileCache::make_file_name(int level, int i, int j) const
{
    std::ostringstream ss;
    ss << tile_dir_;
    ss << level << '/' << i << '/' << j << ".png";

    return ss.str();
}

std::string osmview::TileCache::make_url(int level, int i, int j) const
{
    std::ostringstream ss;
    ss << url_base_;
    ss << level << '/' << i << '/' << j << ".png";

    return ss.str();
}

void osmview::TileCache::request_load(TileCacheItem * item)
{
    fetcher_.emplace([item](){item->load();});
}

void osmview::TileCache::request_download(TileCacheItem * item)
{
    downloader_.emplace([item](){item->download();});
}
