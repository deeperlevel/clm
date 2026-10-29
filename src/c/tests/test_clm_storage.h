#pragma once

#include "test.h"
#include "../clm/storage.h"
#include <string.h>

static int clm_storage_tests(void) {
    clm_test_result result = {0};
    clm_test_begin("CLM - Storage");

    int v=0, w=72;
    CLM_ASSERT(clm_storage_new_2d(100,CLM_BPA_TYPE_INT,0,4,3)==CLM_BPA_SUCCESS,
               "create two-dimensional integer storage");
    CLM_ASSERT(clm_storage_new_2d(100,CLM_BPA_TYPE_INT,0,4,3)==CLM_BPA_FAILURE,
               "reject duplicate storage IDs");
    CLM_ASSERT(clm_storage_get_to(100,0,2,3,&v,sizeof(v))==CLM_BPA_GET_SUCCESS && v==0,
               "initialize integer cells to zero");
    CLM_ASSERT(clm_storage_set_from(100,0,1,2,&w,sizeof(w))==CLM_BPA_SET_SUCCESS,
               "write an integer cell");
    CLM_ASSERT(clm_storage_get_to(100,0,1,2,&v,sizeof(v))==CLM_BPA_GET_SUCCESS && v==72,
               "read the written integer cell");
    CLM_ASSERT(clm_storage_get_to(100,1,1,2,&v,sizeof(v))==CLM_BPA_FAILURE,
               "reject an out-of-bounds layer");
    CLM_ASSERT(clm_storage_get_to(100,0,3,2,&v,sizeof(v))==CLM_BPA_FAILURE,
               "reject an out-of-bounds row");
    CLM_ASSERT(clm_storage_set_from(100,0,1,2,&w,2)==CLM_BPA_FAILURE,
               "reject an incorrect cell buffer size");
    clm_storage_info info;
    CLM_ASSERT(clm_storage_get_info(100,&info)==CLM_BPA_GET_SUCCESS && info.columns==4 && info.rows==3 && info.layers==1,
               "report storage dimensions");
    CLM_ASSERT(clm_storage_slice(100,0,1,1,1,2,2,101)==CLM_BPA_SUCCESS,
               "create a two-dimensional slice");
    CLM_ASSERT(clm_storage_get_to(101,0,0,1,&v,sizeof(v))==CLM_BPA_GET_SUCCESS && v==72,
               "copy source cells into the slice");
    w=21;
    CLM_ASSERT(clm_storage_set_from(101,0,0,1,&w,sizeof(w))==CLM_BPA_SET_SUCCESS,
               "modify a slice cell");
    CLM_ASSERT(clm_storage_get_to(100,0,1,2,&v,sizeof(v))==CLM_BPA_GET_SUCCESS && v==72,
               "keep source cells independent of slice changes");
    CLM_ASSERT(clm_storage_slice(100,0,2,2,1,2,2,102)==CLM_BPA_FAILURE,
               "reject an out-of-bounds slice");
    CLM_ASSERT(clm_storage_delete(101)==CLM_BPA_SUCCESS,
               "delete the two-dimensional slice");
    CLM_ASSERT(clm_storage_delete(100)==CLM_BPA_SUCCESS,
               "delete two-dimensional integer storage");
    CLM_ASSERT(clm_storage_get_info(100,&info)==CLM_BPA_FAILURE,
               "reject metadata lookup for deleted storage");
    CLM_ASSERT(clm_storage_new(110,CLM_BPA_TYPE_BYTE,0,2,3,2)==CLM_BPA_SUCCESS,
               "create three-dimensional byte storage");
    clm_byte x=9, r=0;
    CLM_ASSERT(clm_storage_set_from(110,1,2,1,&x,1)==CLM_BPA_SET_SUCCESS,
               "write a cell in the second layer");
    CLM_ASSERT(clm_storage_get_to(110,0,2,1,&r,1)==CLM_BPA_GET_SUCCESS && r==0,
               "keep cells in different layers independent");
    CLM_ASSERT(clm_storage_slice(110,1,1,0,1,2,2,111)==CLM_BPA_SUCCESS,
               "slice cells from the second layer");
    CLM_ASSERT(clm_storage_get_to(111,0,1,1,&r,1)==CLM_BPA_GET_SUCCESS && r==9,
               "preserve layer cell values in a slice");
    CLM_ASSERT(clm_storage_delete(111)==CLM_BPA_SUCCESS,
               "delete the layer slice");
    CLM_ASSERT(clm_storage_delete(110)==CLM_BPA_SUCCESS,
               "delete three-dimensional byte storage");
    CLM_ASSERT(clm_storage_atlas_new(120,CLM_BPA_TYPE_BYTE,0,4,4,2,2)==CLM_BPA_SUCCESS,
               "create a two-dimensional byte atlas");
    const clm_byte frame[4]={1,2,3,4}; clm_byte read[4]={0};
    CLM_ASSERT(clm_storage_atlas_set_frame_from(120,0,1,1,frame,4)==CLM_BPA_SET_SUCCESS,
               "write an atlas frame");
    CLM_ASSERT(clm_storage_atlas_get_frame_to(120,0,1,1,read,4)==CLM_BPA_GET_SUCCESS && memcmp(frame,read,4)==0,
               "read the written atlas frame");
    CLM_ASSERT(clm_storage_atlas_get_frame_to(120,0,0,0,read,4)==CLM_BPA_GET_SUCCESS && read[0]==0 && read[3]==0,
               "leave unwritten atlas frames zero-initialized");
    CLM_ASSERT(clm_storage_atlas_get_frame_to(120,0,2,0,read,4)==CLM_BPA_FAILURE,
               "reject an out-of-bounds atlas frame");
    CLM_ASSERT(clm_storage_atlas_get_frame_to(120,0,1,1,read,3)==CLM_BPA_FAILURE,
               "reject an incorrect frame buffer size");
    CLM_ASSERT(clm_storage_atlas_slice_frame(120,0,1,1,121)==CLM_BPA_SUCCESS,
               "slice an atlas frame into storage");
    CLM_ASSERT(clm_storage_get_to(121,0,1,1,&r,1)==CLM_BPA_GET_SUCCESS && r==4,
               "preserve cell values in an atlas frame slice");
    CLM_ASSERT(clm_storage_delete(121)==CLM_BPA_SUCCESS,
               "delete the atlas frame slice");
    CLM_ASSERT(clm_storage_delete(120)==CLM_BPA_SUCCESS,
               "delete the two-dimensional atlas");
    CLM_ASSERT(clm_storage_atlas_new(122,CLM_BPA_TYPE_BYTE,0,5,4,2,2)==CLM_BPA_FAILURE,
               "reject atlas dimensions not divisible by frame dimensions");
    CLM_ASSERT(clm_storage_new_2d(123,CLM_BPA_TYPE_STRING,0,2,2)==CLM_BPA_FAILURE,
               "reject unsupported string storage");
    CLM_ASSERT(clm_storage_new_2d(123,CLM_BPA_TYPE_INT,0,0,2)==CLM_BPA_FAILURE,
               "reject zero-sized storage dimensions");
    CLM_ASSERT(clm_storage_new(123,CLM_BPA_TYPE_INT,0,4294967295ULL,4294967295ULL,2)==CLM_BPA_FAILURE,
               "reject overflowing storage dimensions");
    CLM_ASSERT(clm_storage_atlas_new_3d(130,CLM_BPA_TYPE_BYTE,0,4,4,2,2,2)==CLM_BPA_SUCCESS,
               "create a three-dimensional byte atlas");
    CLM_ASSERT(clm_storage_atlas_set_frame_from(130,1,0,1,frame,4)==CLM_BPA_SET_SUCCESS,
               "write an atlas frame in the second layer");
    CLM_ASSERT(clm_storage_atlas_get_frame_to(130,1,0,1,read,4)==CLM_BPA_GET_SUCCESS && memcmp(frame,read,4)==0,
               "read an atlas frame from the second layer");
    CLM_ASSERT(clm_storage_get_to(130,0,0,2,&r,1)==CLM_BPA_GET_SUCCESS && r==0,
               "keep atlas layers independent");
    CLM_ASSERT(clm_storage_delete(130)==CLM_BPA_SUCCESS,
               "delete the three-dimensional atlas");
    CLM_ASSERT(clm_storage_new_2d(140,CLM_BPA_TYPE_INT,0,2,2)==CLM_BPA_SUCCESS,
               "create integer storage for fill and BPA transfers");
    v=5;
    CLM_ASSERT(clm_storage_fill_from(140,&v,sizeof(v))==CLM_BPA_SUCCESS,
               "fill storage with an integer value");
    CLM_ASSERT(clm_storage_get_to(140,0,1,1,&w,sizeof(w))==CLM_BPA_GET_SUCCESS && w==5,
               "read a filled cell");
    CLM_ASSERT(clm_bpa_create(141,CLM_BPA_TYPE_INT)==CLM_BPA_SUCCESS,
               "create a BPA integer for cell transfers");
    v=83;
    CLM_ASSERT(clm_bpa_set(141,&v,sizeof(v))==CLM_BPA_SET_SUCCESS,
               "set the BPA transfer value");
    CLM_ASSERT(clm_storage_set(140,0,0,0,141)==CLM_BPA_SET_SUCCESS,
               "copy a BPA value into a storage cell");
    CLM_ASSERT(clm_storage_get(140,0,0,0,141)==CLM_BPA_GET_SUCCESS,
               "copy a storage cell into a BPA value");
    CLM_ASSERT(clm_bpa_get(141,&w,sizeof(w))==CLM_BPA_GET_SUCCESS && w==83,
               "preserve the integer value through BPA transfers");
    CLM_ASSERT(clm_bpa_delete(141)==CLM_BPA_SUCCESS,
               "delete the BPA transfer value");
    CLM_ASSERT(clm_storage_delete(140)==CLM_BPA_SUCCESS,
               "delete the filled integer storage");
    clm_bpa_shutdown();

    clm_test_summary(result.passed, result.failed);
    
    return result.failed;
}
