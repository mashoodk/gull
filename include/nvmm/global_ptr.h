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

#ifndef _NVMM_GLOBAL_PTR_H_
#define _NVMM_GLOBAL_PTR_H_

#include <assert.h>
#include "nvmm/shelf_id.h"

namespace nvmm{

// GlobalPtr consists of three parts: shelf ID, reserved bytes, and offset
// Shelf ID is usually assigned by the memory manager (or the user)
// Offset is usually determined by the heap implementation, which is the offset into a shelf
// Sometimes the heap may want to encode more info in the offset, so the reserved bytes extend
// offset by one or more bytes; GetReserveAndOffset() returns both reserved bytes and offset as the Offset
template<class GlobalPtrT, size_t GlobalPtrTSize, class ShelfIdT, size_t ShelfIdTSize, class ReserveT, size_t ReserveTSize, class OffsetT, size_t OffsetTSize>
class GlobalPtrClass
{
    static_assert(sizeof(GlobalPtrT)*8 >= GlobalPtrTSize, "Invalid GlobalPtrT");
    static_assert(sizeof(ShelfIdT)*8 >= ShelfIdTSize, "Invalid ShelfIdT");
    static_assert(sizeof(ReserveT)*8 >= ReserveTSize, "Invalid ReserveT");
    static_assert(sizeof(OffsetT)*8 >= OffsetTSize + ReserveTSize, "Invalid OffsetT");
    static_assert(ShelfIdTSize+ReserveTSize+OffsetTSize <= GlobalPtrTSize, "Invalid sizes");
    
    static_assert(GlobalPtrTSize<=64, "GlobalPtr is at most 8 bytes"); // must fit into one uint64_t
    
public:
    GlobalPtrClass()
        : global_ptr_{EncodeGlobalPtr(0, 0)}
    {
    }

    GlobalPtrClass(GlobalPtrT global_ptr)
        : global_ptr_{global_ptr}
    {
    }

    GlobalPtrClass(ShelfIdT shelf_id, OffsetT offset)
        : global_ptr_{EncodeGlobalPtr(shelf_id, offset)}
    {
    }

    GlobalPtrClass(ShelfIdT shelf_id, ReserveT reserve, OffsetT offset)
        : global_ptr_{EncodeGlobalPtr(shelf_id, reserve, offset)}
    {
    }
    
    ~GlobalPtrClass()
    {

    }
    
    inline bool IsValid() const
    {
        return GetShelfId().IsValid() && IsValidOffset(GetOffset());
    }

    inline static bool IsValidOffset(OffsetT offset)
    {
        return offset != 0;
    }

    inline ShelfIdT GetShelfId() const
    {
        return DecodeShelfId(global_ptr_);
    }

    inline OffsetT GetOffset() const
    {
        return DecodeOffset(global_ptr_);
    }

    inline OffsetT GetReserveAndOffset() const
    {
        return DecodeReserveAndOffset(global_ptr_);
    }
    
    inline ReserveT GetReserve() const
    {
        return DecodeReserve(global_ptr_);
    }
    
    inline uint64_t ToUINT64() const
    {
        return (uint64_t)global_ptr_;
    }    
    
    // operators
    friend std::ostream& operator<<(std::ostream& os, const GlobalPtrClass& global_ptr)
    {
        //os << "[" << global_ptr.GetShelfId() << ":" << global_ptr.GetOffset() << "]";
        ShelfId shelf_id = global_ptr.GetShelfId();
        os << "[" << (uint64_t)shelf_id.GetPoolId() << "_" << (uint64_t)shelf_id.GetShelfIndex()
           << ":" << global_ptr.GetOffset() << "]";        
        return os;
    }

    friend bool operator==(const GlobalPtrClass &left, const GlobalPtrClass &right)
    {
        return left.global_ptr_ == right.global_ptr_;
    }   

    friend bool operator!=(const GlobalPtrClass &left, const GlobalPtrClass &right)
    {
        return !(left == right);
    }   
    
private:
    static int const kShelfIdShift = ReserveTSize + OffsetTSize;
    static int const kReserveShift = OffsetTSize;
    
    inline GlobalPtrT EncodeGlobalPtr(ShelfIdT shelf_id, OffsetT offset) const
    {
        return ((GlobalPtrT)shelf_id.GetShelfId() << (kShelfIdShift)) + offset;
    }

    inline GlobalPtrT EncodeGlobalPtr(ShelfIdT shelf_id, ReserveT reserve, OffsetT offset) const
    {
        return ((GlobalPtrT)shelf_id.GetShelfId() << kShelfIdShift) + ((GlobalPtrT)reserve << kReserveShift) + offset;
    }
    
    inline ShelfIdT DecodeShelfId(GlobalPtrT global_ptr) const
    {
        return ShelfIdT((ShelfIdStorageType)(global_ptr >> kShelfIdShift));
    }

    inline ReserveT DecodeReserve(GlobalPtrT global_ptr) const
    {
        ReserveT reserve = (ReserveT)((((GlobalPtrT)1 << ReserveTSize) - 1) << kReserveShift & global_ptr);
        return reserve;
    }
    
    inline OffsetT DecodeOffset(GlobalPtrT global_ptr) const
    {
        OffsetT offset = (OffsetT)((((GlobalPtrT)1 << OffsetTSize) - 1) & global_ptr);
        return offset;
    }

    inline OffsetT DecodeReserveAndOffset(GlobalPtrT global_ptr) const
    {
        OffsetT offset = (OffsetT)((((GlobalPtrT)1 << (OffsetTSize+ReserveTSize)) - 1) & global_ptr);
        return offset;
    }
    
    GlobalPtrT global_ptr_;
};

// internal type of GlobalPtr
using GlobalPtrStorageType = uint64_t;    
    
// Offset
using Offset = uint64_t;

// Reserved bytes
using Reserve = uint8_t;    
    
// GlobalPtr
using GlobalPtr = GlobalPtrClass<GlobalPtrStorageType, 64, ShelfId, 8, Reserve, 8, Offset, 48>;

} // namespace nvmm

#endif