#pragma once

#include "../clm/storage.h"
#include <stdio.h>
#include <string.h>
static int passed=0,failed=0;
#define CHECK(c) do { if(c)++passed; else {++failed; fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#c);} } while(0)

static int clm_storage_tests(void) {
    int v=0, w=72;
    CHECK(clm_storage_new_2d(100,CLM_BPA_TYPE_INT,0,4,3)==CLM_BPA_SUCCESS);
    CHECK(clm_storage_new_2d(100,CLM_BPA_TYPE_INT,0,4,3)==CLM_BPA_FAILURE);
    CHECK(clm_storage_get_to(100,0,2,3,&v,sizeof(v))==CLM_BPA_GET_SUCCESS && v==0);
    CHECK(clm_storage_set_from(100,0,1,2,&w,sizeof(w))==CLM_BPA_SET_SUCCESS);
    CHECK(clm_storage_get_to(100,0,1,2,&v,sizeof(v))==CLM_BPA_GET_SUCCESS && v==72);
    CHECK(clm_storage_get_to(100,1,1,2,&v,sizeof(v))==CLM_BPA_FAILURE);
    CHECK(clm_storage_get_to(100,0,3,2,&v,sizeof(v))==CLM_BPA_FAILURE);
    CHECK(clm_storage_set_from(100,0,1,2,&w,2)==CLM_BPA_FAILURE);
    clm_storage_info info;
    CHECK(clm_storage_get_info(100,&info)==CLM_BPA_GET_SUCCESS && info.columns==4 && info.rows==3 && info.layers==1);
    CHECK(clm_storage_slice(100,0,1,1,1,2,2,101)==CLM_BPA_SUCCESS);
    CHECK(clm_storage_get_to(101,0,0,1,&v,sizeof(v))==CLM_BPA_GET_SUCCESS && v==72);
    w=21;
    CHECK(clm_storage_set_from(101,0,0,1,&w,sizeof(w))==CLM_BPA_SET_SUCCESS);
    CHECK(clm_storage_get_to(100,0,1,2,&v,sizeof(v))==CLM_BPA_GET_SUCCESS && v==72);
    CHECK(clm_storage_slice(100,0,2,2,1,2,2,102)==CLM_BPA_FAILURE);
    CHECK(clm_storage_delete(101)==CLM_BPA_SUCCESS);
    CHECK(clm_storage_delete(100)==CLM_BPA_SUCCESS);
    CHECK(clm_storage_get_info(100,&info)==CLM_BPA_FAILURE);
    CHECK(clm_storage_new(110,CLM_BPA_TYPE_BYTE,0,2,3,2)==CLM_BPA_SUCCESS);
    clm_byte x=9, r=0;
    CHECK(clm_storage_set_from(110,1,2,1,&x,1)==CLM_BPA_SET_SUCCESS);
    CHECK(clm_storage_get_to(110,0,2,1,&r,1)==CLM_BPA_GET_SUCCESS && r==0);
    CHECK(clm_storage_slice(110,1,1,0,1,2,2,111)==CLM_BPA_SUCCESS);
    CHECK(clm_storage_get_to(111,0,1,1,&r,1)==CLM_BPA_GET_SUCCESS && r==9);
    CHECK(clm_storage_delete(111)==CLM_BPA_SUCCESS);
    CHECK(clm_storage_delete(110)==CLM_BPA_SUCCESS);
    CHECK(clm_storage_atlas_new(120,CLM_BPA_TYPE_BYTE,0,4,4,2,2)==CLM_BPA_SUCCESS);
    const clm_byte frame[4]={1,2,3,4}; clm_byte read[4]={0};
    CHECK(clm_storage_atlas_set_frame_from(120,0,1,1,frame,4)==CLM_BPA_SET_SUCCESS);
    CHECK(clm_storage_atlas_get_frame_to(120,0,1,1,read,4)==CLM_BPA_GET_SUCCESS && memcmp(frame,read,4)==0);
    CHECK(clm_storage_atlas_get_frame_to(120,0,0,0,read,4)==CLM_BPA_GET_SUCCESS && read[0]==0 && read[3]==0);
    CHECK(clm_storage_atlas_get_frame_to(120,0,2,0,read,4)==CLM_BPA_FAILURE);
    CHECK(clm_storage_atlas_get_frame_to(120,0,1,1,read,3)==CLM_BPA_FAILURE);
    CHECK(clm_storage_atlas_slice_frame(120,0,1,1,121)==CLM_BPA_SUCCESS);
    CHECK(clm_storage_get_to(121,0,1,1,&r,1)==CLM_BPA_GET_SUCCESS && r==4);
    CHECK(clm_storage_delete(121)==CLM_BPA_SUCCESS);
    CHECK(clm_storage_delete(120)==CLM_BPA_SUCCESS);
    CHECK(clm_storage_atlas_new(122,CLM_BPA_TYPE_BYTE,0,5,4,2,2)==CLM_BPA_FAILURE);
    CHECK(clm_storage_new_2d(123,CLM_BPA_TYPE_STRING,0,2,2)==CLM_BPA_FAILURE);
    CHECK(clm_storage_new_2d(123,CLM_BPA_TYPE_INT,0,0,2)==CLM_BPA_FAILURE);
    CHECK(clm_storage_new(123,CLM_BPA_TYPE_INT,0,4294967295ULL,4294967295ULL,2)==CLM_BPA_FAILURE);
    CHECK(clm_storage_atlas_new_3d(130,CLM_BPA_TYPE_BYTE,0,4,4,2,2,2)==CLM_BPA_SUCCESS);
    CHECK(clm_storage_atlas_set_frame_from(130,1,0,1,frame,4)==CLM_BPA_SET_SUCCESS);
    CHECK(clm_storage_atlas_get_frame_to(130,1,0,1,read,4)==CLM_BPA_GET_SUCCESS && memcmp(frame,read,4)==0);
    CHECK(clm_storage_get_to(130,0,0,2,&r,1)==CLM_BPA_GET_SUCCESS && r==0);
    CHECK(clm_storage_delete(130)==CLM_BPA_SUCCESS);
    CHECK(clm_storage_new_2d(140,CLM_BPA_TYPE_INT,0,2,2)==CLM_BPA_SUCCESS);
    v=5; CHECK(clm_storage_fill_from(140,&v,sizeof(v))==CLM_BPA_SUCCESS);
    CHECK(clm_storage_get_to(140,0,1,1,&w,sizeof(w))==CLM_BPA_GET_SUCCESS && w==5);
    CHECK(clm_bpa_create(141,CLM_BPA_TYPE_INT)==CLM_BPA_SUCCESS);
    v=83; CHECK(clm_bpa_set(141,&v,sizeof(v))==CLM_BPA_SET_SUCCESS);
    CHECK(clm_storage_set(140,0,0,0,141)==CLM_BPA_SET_SUCCESS);
    CHECK(clm_storage_get(140,0,0,0,141)==CLM_BPA_GET_SUCCESS);
    CHECK(clm_bpa_get(141,&w,sizeof(w))==CLM_BPA_GET_SUCCESS && w==83);
    CHECK(clm_bpa_delete(141)==CLM_BPA_SUCCESS);
    CHECK(clm_storage_delete(140)==CLM_BPA_SUCCESS);
    clm_bpa_shutdown();
    printf("CLM Storage: %d passed, %d failed\n",passed,failed);
    return failed?1:0;
}
