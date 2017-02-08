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

#include <fcntl.h> // for O_RDWR
#include <sys/mman.h> // for PROT_READ, PROT_WRITE, MAP_SHARED
#include <pthread.h>

#include <gtest/gtest.h>
#include "nvmm/nvmm_fam_atomic.h"


#include "test_common/test.h"

#include "shelf_mgmt/shelf_name.h"
#include "shelf_usage/shelf_region.h"

using namespace nvmm;

static size_t const kShelfSize = 128*1024*1024LLU; // 128 MB
static ShelfId const kShelfId = ShelfId(1,1);

ShelfName shelf_name;

// single-threaded
// TODO: verify
TEST(ShelfRegion, CreateDestroyVerify)
{

    std::string shelf_path = shelf_name.Path(kShelfId);    
    ShelfFile shelf(shelf_path);
    ShelfRegion region(shelf_path);
    size_t region_size = kShelfSize;

    // create a shelf
    EXPECT_EQ(NO_ERROR, shelf.Create(S_IRUSR | S_IWUSR));
    
    // create a shelf region
    EXPECT_EQ(NO_ERROR, region.Create(region_size));
    EXPECT_EQ(NO_ERROR, region.Verify());
    
    // destroy the region
    EXPECT_EQ(NO_ERROR, region.Destroy());

    // destroy the shelf
    EXPECT_EQ(NO_ERROR, shelf.Destroy());
}

TEST(ShelfRegion, OpenCloseSize)
{

    std::string shelf_path = shelf_name.Path(kShelfId);    
    ShelfFile shelf(shelf_path);
    ShelfRegion region(shelf_path);
    size_t region_size = kShelfSize;

    // open a shelf region that does not exist
    EXPECT_EQ(SHELF_FILE_NOT_FOUND, region.Open(O_RDWR));
    
    // create a shelf
    EXPECT_EQ(NO_ERROR, shelf.Create(S_IRUSR | S_IWUSR));
    
    // create a shelf region
    EXPECT_EQ(NO_ERROR, region.Create(region_size));
    
    // open the region
    EXPECT_EQ(NO_ERROR, region.Open(O_RDWR));

    // check its size
    EXPECT_EQ(region_size, region.Size());

    // close the region
    EXPECT_EQ(NO_ERROR, region.Close());
    
    // destroy the region
    EXPECT_EQ(NO_ERROR, region.Destroy());

    // destroy the shelf
    EXPECT_EQ(NO_ERROR, shelf.Destroy());    
}

TEST(ShelfRegion, MapUnmap)
{

    ErrorCode ret = NO_ERROR;
    std::string shelf_path = shelf_name.Path(kShelfId);    
    ShelfFile shelf(shelf_path);
    ShelfRegion region(shelf_path);
    size_t region_size = kShelfSize;

    // create a shelf
    ret = shelf.Create(S_IRUSR | S_IWUSR);
    EXPECT_EQ(NO_ERROR, ret);
 
    // create a shelf region
    ret = region.Create(region_size);
    EXPECT_EQ(NO_ERROR, ret);

    int64_t* address = NULL;
    
    // write a value
    EXPECT_EQ(NO_ERROR, region.Open(O_RDWR));
    EXPECT_EQ(NO_ERROR, region.Map(NULL, region_size, PROT_READ|PROT_WRITE, MAP_SHARED, 0, (void**)&address));
    fam_atomic_64_write(address, 123LL);
    EXPECT_EQ(NO_ERROR, region.Unmap(address, region_size));
    EXPECT_EQ(NO_ERROR, region.Close());        

    // read it back
    EXPECT_EQ(NO_ERROR, region.Open(O_RDWR));
    EXPECT_EQ(NO_ERROR, region.Map(NULL, region_size, PROT_READ|PROT_WRITE, MAP_SHARED, 0, (void**)&address));
    EXPECT_EQ(123LL, fam_atomic_64_read(address));
    EXPECT_EQ(NO_ERROR, region.Unmap(address, region_size));
    EXPECT_EQ(NO_ERROR, region.Close());        

    // destroy the region
    ret = region.Destroy(); 
    EXPECT_EQ(NO_ERROR, ret);
   
    // destroy the shelf
    ret = shelf.Destroy();
    EXPECT_EQ(NO_ERROR, ret);
}

int main(int argc, char** argv)
{
    InitTest();    
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

