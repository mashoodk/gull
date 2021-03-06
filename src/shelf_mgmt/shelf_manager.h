/*
 *  (c) Copyright 2016-2017 Hewlett Packard Enterprise Development Company LP.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  As an exception, the copyright holders of this Library grant you permission
 *  to (i) compile an Application with the Library, and (ii) distribute the
 *  Application containing code generated by the Library and added to the
 *  Application during this compilation process under terms of your choice,
 *  provided you also meet the terms and conditions of the Application license.
 *
 */

#ifndef _NVMM_SHELF_MANAGER_H_
#define _NVMM_SHELF_MANAGER_H_

#include <mutex>
#include <map>
#include <unordered_map>
#include <stddef.h>

#include "nvmm/shelf_id.h"

namespace nvmm{

// the ShelfManager keeps two mappings:
// - shelf ID => base ptr and length
// - base ptr => shelf ID and length    
// NOTE: to register for a shelf, the shelf must be mapped entirely (no partial mmap)
// TODO: to unregister a shelf safely, we have to make sure the shelf is not being accessed
// use reference counts???
// TODO: should also store file handle    
class ShelfManager
{
public:
    /*
      called by ShelfFile
    */
    // register a shelf's ID, base ptr, and length
    static void *RegisterShelf(ShelfId shelf_id, void *base, size_t length);
    // unregister a shelf's ID, base ptr, and length
    // for now we only unregister a shelf when the shelf is being destroyed (deleted)
    static void *UnregisterShelf(ShelfId shelf_id);
    // check if a shelf is registered or not and return its base if it is registered
    static void *LookupShelf(ShelfId shelf_id);

    /*
      called by MemoryManager
    */
    // given a shelf's ID, return the base of the shelf
    static void *FindBase(ShelfId shelf_id);
    // given a shelf's pathname and shelf ID, register it, map it, and return the base of the shelf
    static void *FindBase(std::string path, ShelfId shelf_id);
    // given a local pointer backed by a shelf, return the shelf's ID and its base pointer
    static ShelfId FindShelf(void *ptr, void *&base);
    // unmap everything, clear both mappings
    static void Reset();

    
    static void Lock();
    static void Unlock();    

private:
    static std::mutex map_mutex_; // guard concurrent access to map_ and rmap_ (more specifically,
                                  // mapping/unmapping/finding shelves)
    // shelf ID => base ptr and length
    static std::unordered_map<ShelfId, std::pair<void *, size_t>, ShelfId::Hash, ShelfId::Equal> map_;
    // base ptr => shelf ID and length
    static std::map<void*, std::pair<ShelfId, size_t>> reverse_map_;
    
};

} // namespace nvmm
    
#endif
