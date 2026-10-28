# frozen_string_literal: true

require 'minitest'
require_relative '../clm/storage'

class ClmStorageTest < CLMTestCase
  CLM_TEST_TITLE = 'CLM - Storage'

  def teardown
    CLM::BPA.shutdown
  end

  def test_grid_access_info_slice_and_layering
    assert_equal CLM::BPA::SUCCESS, CLM::Storage.new_2d(100, CLM::BPA::TYPE_INT, 0, 4, 3)
    assert_equal CLM::BPA::FAILURE, CLM::Storage.new_2d(100, CLM::BPA::TYPE_INT, 0, 4, 3)
    assert_equal [CLM::BPA::GET_SUCCESS, 0], CLM::Storage.get_to(100, 0, 2, 3)
    assert_equal CLM::BPA::SET_SUCCESS, CLM::Storage.set_from(100, 0, 1, 2, 72, 4)
    assert_equal [CLM::BPA::GET_SUCCESS, 72], CLM::Storage.get_to(100, 0, 1, 2)
    assert_equal [CLM::BPA::FAILURE, nil], CLM::Storage.get_to(100, 1, 1, 2)
    assert_equal CLM::BPA::FAILURE, CLM::Storage.set_from(100, 0, 1, 2, 72, 2)

    status, info = CLM::Storage.get_info(100)
    assert_equal CLM::BPA::GET_SUCCESS, status
    assert_equal [4, 3, 1], [info.columns, info.rows, info.layers]

    assert_equal CLM::BPA::SUCCESS, CLM::Storage.slice(100, 0, 1, 1, 1, 2, 2, 101)
    assert_equal [CLM::BPA::GET_SUCCESS, 72], CLM::Storage.get_to(101, 0, 0, 1)
    assert_equal CLM::BPA::SET_SUCCESS, CLM::Storage.set_from(101, 0, 0, 1, 21, 4)
    assert_equal [CLM::BPA::GET_SUCCESS, 72], CLM::Storage.get_to(100, 0, 1, 2)
    assert_equal CLM::BPA::FAILURE, CLM::Storage.slice(100, 0, 2, 2, 1, 2, 2, 102)
    assert_equal CLM::BPA::SUCCESS, CLM::Storage.delete(101)
    assert_equal CLM::BPA::SUCCESS, CLM::Storage.delete(100)
    assert_equal [CLM::BPA::FAILURE, nil], CLM::Storage.get_info(100)
  end

  def test_3d_storage_and_block_transfer
    assert_equal CLM::BPA::SUCCESS, CLM::Storage.new(110, CLM::BPA::TYPE_BYTE, 0, 2, 3, 2)
    assert_equal CLM::BPA::SET_SUCCESS, CLM::Storage.set_from(110, 1, 2, 1, 9, 1)
    assert_equal [CLM::BPA::GET_SUCCESS, 0], CLM::Storage.get_to(110, 0, 2, 1)
    assert_equal CLM::BPA::SUCCESS, CLM::Storage.slice(110, 1, 1, 0, 1, 2, 2, 111)
    assert_equal [CLM::BPA::GET_SUCCESS, 9], CLM::Storage.get_to(111, 0, 1, 1)
    assert_equal CLM::BPA::SUCCESS, CLM::Storage.delete(111)
    assert_equal CLM::BPA::SUCCESS, CLM::Storage.delete(110)

    assert_equal CLM::BPA::SUCCESS, CLM::Storage.new_2d(140, CLM::BPA::TYPE_INT, 0, 2, 2)
    assert_equal CLM::BPA::SUCCESS, CLM::Storage.fill_from(140, 5, 4)
    assert_equal [CLM::BPA::GET_SUCCESS, 5], CLM::Storage.get_to(140, 0, 1, 1)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.create(141, CLM::BPA::TYPE_INT)
    assert_equal CLM::BPA::SET_SUCCESS, CLM::BPA.set(141, 83)
    assert_equal CLM::BPA::SET_SUCCESS, CLM::Storage.set(140, 0, 0, 0, 141)
    assert_equal CLM::BPA::GET_SUCCESS, CLM::Storage.get(140, 0, 0, 0, 141)
    assert_equal [CLM::BPA::GET_SUCCESS, 83], CLM::BPA.get(141)
    assert_equal CLM::BPA::SUCCESS, CLM::Storage.delete(140)
    assert_equal CLM::BPA::SUCCESS, CLM::BPA.delete(141)
  end

  def test_atlas_frames_and_validation
    assert_equal CLM::BPA::SUCCESS, CLM::Storage.atlas_new(120, CLM::BPA::TYPE_BYTE, 0, 4, 4, 2, 2)
    frame = [1, 2, 3, 4]
    assert_equal CLM::BPA::SET_SUCCESS, CLM::Storage.atlas_set_frame_from(120, 0, 1, 1, frame, 1)
    assert_equal [CLM::BPA::GET_SUCCESS, frame], CLM::Storage.atlas_get_frame_to(120, 0, 1, 1)
    assert_equal [CLM::BPA::GET_SUCCESS, [0, 0, 0, 0]], CLM::Storage.atlas_get_frame_to(120, 0, 0, 0)
    assert_equal [CLM::BPA::FAILURE, nil], CLM::Storage.atlas_get_frame_to(120, 0, 2, 0)
    assert_equal CLM::BPA::FAILURE, CLM::Storage.atlas_set_frame_from(120, 0, 0, 0, [1, 2], 1)
    assert_equal CLM::BPA::SUCCESS, CLM::Storage.atlas_slice_frame(120, 0, 1, 1, 121)
    assert_equal [CLM::BPA::GET_SUCCESS, 4], CLM::Storage.get_to(121, 0, 1, 1)
    assert_equal CLM::BPA::SUCCESS, CLM::Storage.delete(121)
    assert_equal CLM::BPA::SUCCESS, CLM::Storage.delete(120)

    assert_equal CLM::BPA::FAILURE, CLM::Storage.atlas_new(122, CLM::BPA::TYPE_BYTE, 0, 5, 4, 2, 2)
    assert_equal CLM::BPA::FAILURE, CLM::Storage.new_2d(123, CLM::BPA::TYPE_STRING, 0, 2, 2)
    assert_equal CLM::BPA::FAILURE, CLM::Storage.new_2d(123, CLM::BPA::TYPE_INT, 0, 0, 2)
    assert_equal CLM::BPA::FAILURE, CLM::Storage.new(123, CLM::BPA::TYPE_INT, 0, 0xffff_ffff, 0xffff_ffff, 2)

    assert_equal CLM::BPA::SUCCESS, CLM::Storage.atlas_new_3d(130, CLM::BPA::TYPE_BYTE, 0, 4, 4, 2, 2, 2)
    assert_equal CLM::BPA::SET_SUCCESS, CLM::Storage.atlas_set_frame_from(130, 1, 0, 1, frame, 1)
    assert_equal [CLM::BPA::GET_SUCCESS, frame], CLM::Storage.atlas_get_frame_to(130, 1, 0, 1)
    assert_equal [CLM::BPA::GET_SUCCESS, 0], CLM::Storage.get_to(130, 0, 0, 2)
    assert_equal CLM::BPA::SUCCESS, CLM::Storage.delete(130)
  end
end