module tests.storage_test;

import clm.storage;
import tests.test;

int clmStorageTests()
{
    ClmTestResult result;
    clmTestBegin("CLM - Storage");

    int value;
    int written = 72;
    clmAssert(result, clm_storage_new_2d(100, CLM_BPA_TYPE_INT, 0, 4, 3) == CLM_BPA_SUCCESS,
        "create a 2D integer storage block");
    clmAssert(result, clm_storage_new_2d(100, CLM_BPA_TYPE_INT, 0, 4, 3) == CLM_BPA_FAILURE,
        "reject a duplicate storage ID");
    clmAssert(result, clm_storage_get_to(100, 0, 2, 3, value) == CLM_BPA_GET_SUCCESS &&
        value == 0, "new cells are zero-initialized");
    clmAssert(result, clm_storage_set_from(100, 0, 1, 2, written) == CLM_BPA_SET_SUCCESS &&
        clm_storage_get_to(100, 0, 1, 2, value) == CLM_BPA_GET_SUCCESS && value == 72,
        "write and read a cell");
    clmAssert(result, clm_storage_get_to(100, 1, 1, 2, value) == CLM_BPA_FAILURE,
        "reject an invalid layer");
    clmAssert(result, clm_storage_get_to(100, 0, 3, 2, value) == CLM_BPA_FAILURE,
        "reject an out-of-range column");
    clmAssert(result, clm_storage_set_from(100, 0, 1, 2, written, 2) == CLM_BPA_FAILURE,
        "reject a value with the wrong byte size");

    StorageInfo info;
    clmAssert(result, clm_storage_get_info(100, info) == CLM_BPA_GET_SUCCESS &&
        info.columns == 4 && info.rows == 3 && info.layers == 1,
        "report storage dimensions");
    clmAssert(result, clm_storage_slice(100, 0, 1, 1, 1, 2, 2, 101) == CLM_BPA_SUCCESS &&
        clm_storage_get_to(101, 0, 0, 1, value) == CLM_BPA_GET_SUCCESS && value == 72,
        "copy a rectangular storage slice");
    written = 21;
    clmAssert(result, clm_storage_set_from(101, 0, 0, 1, written) == CLM_BPA_SET_SUCCESS &&
        clm_storage_get_to(100, 0, 1, 2, value) == CLM_BPA_GET_SUCCESS && value == 72,
        "keep copied slices independent from their source");
    clmAssert(result, clm_storage_slice(100, 0, 2, 2, 1, 2, 2, 102) == CLM_BPA_FAILURE,
        "reject a slice extending beyond storage bounds");
    clmAssert(result, clm_storage_delete(101) == CLM_BPA_SUCCESS &&
        clm_storage_delete(100) == CLM_BPA_SUCCESS &&
        clm_storage_get_info(100, info) == CLM_BPA_FAILURE,
        "delete storage and reject subsequent lookup");

    clmAssert(result, clm_storage_new(110, CLM_BPA_TYPE_BYTE, 0, 2, 3, 2) == CLM_BPA_SUCCESS,
        "create layered byte storage");
    ubyte byteValue = 9;
    ubyte byteRead;
    clmAssert(result, clm_storage_set_from(110, 1, 2, 1, byteValue) == CLM_BPA_SET_SUCCESS &&
        clm_storage_get_to(110, 0, 2, 1, byteRead) == CLM_BPA_GET_SUCCESS && byteRead == 0,
        "keep layers independently initialized");
    clmAssert(result, clm_storage_slice(110, 1, 1, 0, 1, 2, 2, 111) == CLM_BPA_SUCCESS &&
        clm_storage_get_to(111, 0, 1, 1, byteRead) == CLM_BPA_GET_SUCCESS && byteRead == 9,
        "slice a selected layer");
    clmAssert(result, clm_storage_delete(111) == CLM_BPA_SUCCESS &&
        clm_storage_delete(110) == CLM_BPA_SUCCESS, "delete layered storage");

    clmAssert(result, clm_storage_atlas_new(120, CLM_BPA_TYPE_BYTE, 0, 4, 4, 2, 2) == CLM_BPA_SUCCESS,
        "create a 2D frame atlas");
    ubyte[] frame = [1, 2, 3, 4];
    ubyte[] readFrame = new ubyte[4];
    clmAssert(result, clm_storage_atlas_set_frame_from(120, 0, 1, 1, frame) == CLM_BPA_SET_SUCCESS &&
        clm_storage_atlas_get_frame_to(120, 0, 1, 1, readFrame) == CLM_BPA_GET_SUCCESS &&
        readFrame == frame, "write and read an atlas frame");
    clmAssert(result, clm_storage_atlas_get_frame_to(120, 0, 0, 0, readFrame) == CLM_BPA_GET_SUCCESS &&
        readFrame[0] == 0 && readFrame[3] == 0, "new atlas frames are zero-initialized");
    clmAssert(result, clm_storage_atlas_get_frame_to(120, 0, 2, 0, readFrame) == CLM_BPA_FAILURE,
        "reject an out-of-range atlas coordinate");
    clmAssert(result, clm_storage_atlas_get_frame_to(120, 0, 1, 1, readFrame[0 .. 3]) == CLM_BPA_FAILURE,
        "reject a frame buffer of the wrong size");
    clmAssert(result, clm_storage_atlas_slice_frame(120, 0, 1, 1, 121) == CLM_BPA_SUCCESS &&
        clm_storage_get_to(121, 0, 1, 1, byteRead) == CLM_BPA_GET_SUCCESS && byteRead == 4,
        "extract one atlas frame as storage");
    clmAssert(result, clm_storage_delete(121) == CLM_BPA_SUCCESS &&
        clm_storage_delete(120) == CLM_BPA_SUCCESS, "delete atlas resources");

    clmAssert(result, clm_storage_atlas_new(122, CLM_BPA_TYPE_BYTE, 0, 5, 4, 2, 2) == CLM_BPA_FAILURE,
        "reject atlas dimensions not divisible by frame dimensions");
    clmAssert(result, clm_storage_new_2d(123, CLM_BPA_TYPE_STRING, 0, 2, 2) == CLM_BPA_FAILURE,
        "reject unsupported string-cell storage");
    clmAssert(result, clm_storage_new_2d(123, CLM_BPA_TYPE_INT, 0, 0, 2) == CLM_BPA_FAILURE,
        "reject zero-sized storage");
    clmAssert(result, clm_storage_new(123, CLM_BPA_TYPE_INT, 0, uint.max, uint.max, 2) == CLM_BPA_FAILURE,
        "reject dimensions whose allocation size overflows");

    clmAssert(result, clm_storage_atlas_new_3d(130, CLM_BPA_TYPE_BYTE, 0, 4, 4, 2, 2, 2) == CLM_BPA_SUCCESS,
        "create a 3D frame atlas");
    clmAssert(result, clm_storage_atlas_set_frame_from(130, 1, 0, 1, frame) == CLM_BPA_SET_SUCCESS &&
        clm_storage_atlas_get_frame_to(130, 1, 0, 1, readFrame) == CLM_BPA_GET_SUCCESS &&
        readFrame == frame && clm_storage_get_to(130, 0, 0, 2, byteRead) == CLM_BPA_GET_SUCCESS &&
        byteRead == 0, "read and isolate frames across atlas layers");
    clmAssert(result, clm_storage_delete(130) == CLM_BPA_SUCCESS, "delete 3D atlas");

    clmAssert(result, clm_storage_new_2d(140, CLM_BPA_TYPE_INT, 0, 2, 2) == CLM_BPA_SUCCESS &&
        clm_storage_fill_from(140, 5) == CLM_BPA_SUCCESS &&
        clm_storage_get_to(140, 0, 1, 1, written) == CLM_BPA_GET_SUCCESS && written == 5,
        "fill every cell with one value");
    int blockValue = 83;
    ubyte[] fetchedBlock;
    clmAssert(result, clm_bpa_create(141, CLM_BPA_TYPE_INT) == CLM_BPA_SUCCESS &&
        clm_bpa_set(141, (cast(ubyte*)&blockValue)[0 .. int.sizeof]) == CLM_BPA_SET_SUCCESS &&
        clm_storage_set(140, 0, 0, 0, 141) == CLM_BPA_SET_SUCCESS &&
        clm_storage_get(140, 0, 0, 0, 141) == CLM_BPA_GET_SUCCESS &&
        clm_bpa_get(141, fetchedBlock, int.sizeof) == CLM_BPA_GET_SUCCESS &&
        *cast(int*)fetchedBlock.ptr == 83,
        "transfer values through a compatible BPA block");
    clmAssert(result, clm_bpa_delete(141) == CLM_BPA_SUCCESS &&
        clm_storage_delete(140) == CLM_BPA_SUCCESS, "release final storage resources");
    clm_bpa_shutdown();

    clmTestSummary(result.passed, result.failed);
    return result.failed;
}
