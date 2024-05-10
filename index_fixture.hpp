/*
 * Copyright 2021 Database Group, Nagoya University
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef INDEX_FIXTURES_INDEX_FIXTURE_HPP
#define INDEX_FIXTURES_INDEX_FIXTURE_HPP

// C++ standard libraries
#include <algorithm>
#include <memory>
#include <random>
#include <tuple>
#include <utility>
#include <vector>

// external sources
#include "gtest/gtest.h"

// local sources
#include "common.hpp"

namespace dbgroup::index::test
{
/*######################################################################################
 * Fixture class definition
 *####################################################################################*/

template <class IndexInfo>
class IndexFixture : public testing::Test
{
  /*####################################################################################
   * Type aliases
   *##################################################################################*/

  // extract key-payload types
  using Key = typename IndexInfo::Key::Data;
  using Payload = typename IndexInfo::Payload::Data;
  using KeyComp = typename IndexInfo::Key::Comp;
  using PayComp = typename IndexInfo::Payload::Comp;
  using Index_t = typename IndexInfo::Index_t;
  using ImplStat = typename IndexInfo::ImplStatus;
  using ScanKey = std::optional<std::tuple<const Key &, size_t, bool>>;
  using ScanKeyRef = std::optional<std::pair<size_t, bool>>;

  using EpochManager = ::dbgroup::memory::EpochManager;

 protected:
  /*####################################################################################
   * Internal constants
   *##################################################################################*/

  static constexpr size_t kRecNumWithoutSMOs = 30;
  static constexpr size_t kRecNumWithLeafSMOs = 1000;
  static constexpr size_t kRecNumWithInternalSMOs = 30000;
  static constexpr size_t kKeyNum = kExecNum + 2;

  /*####################################################################################
   * Setup/Teardown
   *##################################################################################*/

  void
  SetUp() override
  {
    keys_ = PrepareTestData<Key>(kKeyNum);
    payloads_ = PrepareTestData<Payload>(kKeyNum);

    auto epoch_manager = std::make_shared<EpochManager>();
    epoch_manager_ = epoch_manager;
    index_ = std::make_unique<Index_t>(epoch_manager);
  }

  void
  TearDown() override
  {
    index_ = nullptr;
  }

  /*####################################################################################
   * Utility functions
   *##################################################################################*/

  void
  PrepareData()
  {
    keys_ = PrepareTestData<Key>(kKeyNum);
    payloads_ = PrepareTestData<Payload>(kKeyNum);
  }

  void
  DestroyData()
  {
    ReleaseTestData(keys_);
    ReleaseTestData(payloads_);
  }

  [[nodiscard]] auto
  CreateTargetIDs(  //
      const size_t rec_num,
      const AccessPattern pattern) const  //
      -> std::vector<size_t>
  {
    std::mt19937_64 rand_engine{kRandomSeed};

    std::vector<size_t> target_ids{};
    target_ids.reserve(rec_num);
    if (pattern == kReverse) {
      for (int64_t i = rec_num - 1; i >= 0; --i) {
        target_ids.emplace_back(i);
      }
    } else {
      for (size_t i = 0; i < rec_num; ++i) {
        target_ids.emplace_back(i);
      }
    }

    if (pattern == kRandom) {
      std::shuffle(target_ids.begin(), target_ids.end(), rand_engine);
    }

    return target_ids;
  }

  auto
  Write(  //
      [[maybe_unused]] const size_t key_id,
      [[maybe_unused]] const size_t pay_id)
  {
    if constexpr (HasWriteOperation<ImplStat>()) {
      const auto &key = keys_.at(key_id);
      const auto &payload = payloads_.at(pay_id);
      return index_->Write(key, payload, GetLength(key), GetLength(payload));
    } else {
      return 0;
    }
  }

  auto
  Insert(  //
      [[maybe_unused]] const size_t key_id,
      [[maybe_unused]] const size_t pay_id)
  {
    if constexpr (HasInsertOperation<ImplStat>()) {
      const auto &key = keys_.at(key_id);
      const auto &payload = payloads_.at(pay_id);
      return index_->Insert(key, payload, GetLength(key), GetLength(payload));
    } else {
      return 0;
    }
  }

  auto
  Update(  //
      [[maybe_unused]] const size_t key_id,
      [[maybe_unused]] const size_t pay_id)
  {
    if constexpr (HasUpdateOperation<ImplStat>()) {
      const auto &key = keys_.at(key_id);
      const auto &payload = payloads_.at(pay_id);
      return index_->Update(key, payload, GetLength(key), GetLength(payload));
    } else {
      return 0;
    }
  }

  auto
  Delete([[maybe_unused]] const size_t key_id)
  {
    if constexpr (HasDeleteOperation<ImplStat>()) {
      const auto &key = keys_.at(key_id);
      return index_->Delete(key, GetLength(key));
    } else {
      return 0;
    }
  }

  void
  FillIndex()
  {
    for (size_t i = 0; i < kExecNum; ++i) {
      if constexpr (HasWriteOperation<ImplStat>()) {
        Write(i, i);
      } else {
        Insert(i, i);
      }
    }
  }

  /*####################################################################################
   * Functions for verification
   *##################################################################################*/
  void VerifySnapshotRead(  //

  )
  {
    const auto &target_ids = CreateTargetIDs(kExecNum, kSequential);
    VerifyWrite(target_ids, !kWriteTwice);
    epoch_manager_->ForwardGlobalEpoch();
    const auto &[epoch_guard, protected_epochs] = epoch_manager_->GetProtectedEpochs();

    VerifyWrite(target_ids, kWriteTwice);

    for (size_t i = 0; i < target_ids.size(); ++i) {
      const auto key_id = target_ids.at(i);
      const auto pay_id = key_id;  // i.e., expect to read first writes.

      const auto &key = keys_.at(key_id);
      const auto read_val =
          index_->SnapshotRead(key, epoch_guard, protected_epochs, GetLength(key));

      EXPECT_TRUE(read_val);

      const auto expected_val = payloads_.at(pay_id);
      const auto actual_val = read_val.value();
      EXPECT_TRUE(IsEqual<PayComp>(expected_val, actual_val));
    }
  }

  void
  VerifyRead(  //
      const std::vector<size_t> &target_ids,
      const bool expect_success,
      const bool write_twice = false)
  {
    for (size_t i = 0; i < target_ids.size(); ++i) {
      const auto key_id = target_ids.at(i);
      const auto pay_id = (write_twice) ? key_id + 1 : key_id;

      const auto &key = keys_.at(key_id);
      const auto read_val = index_->Read(key, GetLength(key));
      if (expect_success) {
        EXPECT_TRUE(read_val);

        const auto expected_val = payloads_.at(pay_id);
        const auto actual_val = read_val.value();
        EXPECT_TRUE(IsEqual<PayComp>(expected_val, actual_val));
      } else {
        EXPECT_FALSE(read_val);
      }
    }
  }

  void
  VerifyScan(  //
      [[maybe_unused]] const ScanKeyRef &begin_ref,
      [[maybe_unused]] const ScanKeyRef &end_ref,
      [[maybe_unused]] const bool expect_success = true,
      [[maybe_unused]] const bool write_twice = false)
  {
    epoch_manager_->ForwardGlobalEpoch();
    const auto &[epoch_guard, protected_epochs] = epoch_manager_->GetProtectedEpochs();

    if constexpr (HasScanOperation<ImplStat>()) {
      ScanKey begin_key = std::nullopt;
      size_t begin_pos = 0;
      if (begin_ref) {
        auto &&[begin_id, begin_closed] = *begin_ref;
        const auto &key = keys_.at(begin_id);
        begin_key.emplace(key, GetLength(key), begin_closed);
        begin_pos = (begin_closed) ? begin_id : begin_id + 1;
      }

      ScanKey end_key = std::nullopt;
      size_t end_pos = 0;
      if (end_ref) {
        auto &&[end_id, end_closed] = *end_ref;
        const auto &key = keys_.at(end_id);
        end_key.emplace(key, GetLength(key), end_closed);
        end_pos = (end_closed) ? end_id + 1 : end_id;
      }

      auto &&iter = index_->Scan(epoch_guard, protected_epochs, begin_key, end_key);
      if (expect_success) {
        for (; iter; ++iter, ++begin_pos) {
          const auto &[key, payload] = *iter;
          const auto val_id = (write_twice) ? begin_pos + 1 : begin_pos;
          EXPECT_TRUE(IsEqual<KeyComp>(keys_.at(begin_pos), key));
          EXPECT_TRUE(IsEqual<PayComp>(payloads_.at(val_id), payload));
        }
        if (end_ref) {
          EXPECT_EQ(begin_pos, end_pos);
        }
      }
      EXPECT_FALSE(iter);
    }
  }

  void
  VerifyWrite(  //
      const std::vector<size_t> &target_ids,
      const bool write_twice = false)
  {
    for (size_t i = 0; i < target_ids.size(); ++i) {
      const auto key_id = target_ids.at(i);
      const auto pay_id = (write_twice) ? key_id + 1 : key_id;

      const auto rc = Write(key_id, pay_id);
      EXPECT_EQ(rc, 0);
    }
  }

  void
  VerifyInsert(  //
      const std::vector<size_t> &target_ids,
      const bool expect_success,
      const bool write_twice = false)
  {
    for (size_t i = 0; i < target_ids.size(); ++i) {
      const auto key_id = target_ids.at(i);
      const auto pay_id = (write_twice) ? key_id + 1 : key_id;

      const auto rc = Insert(key_id, pay_id);
      if (expect_success) {
        EXPECT_EQ(rc, 0);
      } else {
        EXPECT_NE(rc, 0);
      }
    }
  }

  void
  VerifyUpdate(  //
      const std::vector<size_t> &target_ids,
      const bool expect_success)
  {
    for (size_t i = 0; i < target_ids.size(); ++i) {
      const auto key_id = target_ids.at(i);
      const auto pay_id = key_id + 1;

      const auto rc = Update(key_id, pay_id);
      if (expect_success) {
        EXPECT_EQ(rc, 0);
      } else {
        EXPECT_NE(rc, 0);
      }
    }
  }

  void
  VerifyDelete(  //
      const std::vector<size_t> &target_ids,
      const bool expect_success)
  {
    for (size_t i = 0; i < target_ids.size(); ++i) {
      const auto key_id = target_ids.at(i);

      const auto rc = Delete(key_id);
      if (expect_success) {
        EXPECT_EQ(rc, 0);
      } else {
        EXPECT_NE(rc, 0);
      }
    }
  }

  void
  VerifyBulkload()
  {
    if constexpr (HasBulkloadOperation<ImplStat>()) {
      if constexpr (IsVarLen<Key>() || IsVarLen<Payload>()) {
        std::vector<std::tuple<Key, Payload, size_t, size_t>> entries{};
        entries.reserve(kExecNum);
        for (size_t i = 0; i < kExecNum; ++i) {
          const auto &key = keys_.at(i);
          const auto &payload = payloads_.at(i);
          entries.emplace_back(key, payload, GetLength(key), GetLength(payload));
        }

        const auto rc = index_->Bulkload(entries, 1);
        EXPECT_EQ(rc, 0);
      } else {
        std::vector<std::pair<Key, Payload>> entries{};
        entries.reserve(kExecNum);
        for (size_t i = 0; i < kExecNum; ++i) {
          entries.emplace_back(keys_.at(i), payloads_.at(i));
        }

        const auto rc = index_->Bulkload(entries, 1);
        EXPECT_EQ(rc, 0);
      }
    }
  }

  /*####################################################################################
   * Functions for test definitions
   *##################################################################################*/

  void
  VerifyReadEmptyIndex()
  {
    PrepareData();
    VerifyRead({0}, kExpectFailed);
    DestroyData();
  }

  void
  VerifyScanWith(  //
      const bool has_range,
      const bool closed = true)
  {
    constexpr auto kRecNum = kRecNumWithInternalSMOs;

    if (!HasScanOperation<ImplStat>()                                            //
        || (!HasWriteOperation<ImplStat>() && !HasInsertOperation<ImplStat>()))  //
    {
      GTEST_SKIP();
    }

    PrepareData();

    ScanKeyRef begin_key = std::nullopt;
    ScanKeyRef end_key = std::nullopt;
    if (has_range) {
      begin_key = std::make_pair(0, closed);
      end_key = std::make_pair(kRecNum - 1, closed);
    }

    FillIndex();
    VerifyScan(begin_key, end_key);

    DestroyData();
  }

  void
  VerifyWritesWith(  //
      const bool write_twice,
      const bool with_delete,
      const AccessPattern pattern,
      const size_t ops_num = kExecNum)
  {
    if (!HasWriteOperation<ImplStat>()                        //
        || (with_delete && !HasDeleteOperation<ImplStat>()))  //
    {
      GTEST_SKIP();
    }

    PrepareData();

    const auto &target_ids = CreateTargetIDs(ops_num, pattern);
    const auto &begin_ref = std::make_pair(0, kRangeClosed);
    const auto &end_ref = std::make_pair(ops_num, kRangeOpened);

    VerifyWrite(target_ids);
    if (with_delete) VerifyDelete(target_ids, kExpectSuccess);
    if (write_twice) VerifyWrite(target_ids, kWriteTwice);
    VerifyRead(target_ids, !with_delete || write_twice, write_twice);
    VerifyScan(begin_ref, end_ref, kExpectSuccess, write_twice);

    DestroyData();
  }

  void
  VerifyInsertsWith(  //
      const bool write_twice,
      const bool with_delete,
      const AccessPattern pattern)
  {
    if (!HasInsertOperation<ImplStat>()                       //
        || (with_delete && !HasDeleteOperation<ImplStat>()))  //
    {
      GTEST_SKIP();
    }

    PrepareData();

    const auto &target_ids = CreateTargetIDs(kExecNum, pattern);
    const auto &begin_ref = std::make_pair(0, kRangeClosed);
    const auto &end_ref = std::make_pair(kExecNum, kRangeOpened);
    const auto expect_success = !with_delete || write_twice;
    const bool is_updated = write_twice && with_delete;

    VerifyInsert(target_ids, kExpectSuccess);
    if (with_delete) VerifyDelete(target_ids, kExpectSuccess);
    if (write_twice) VerifyInsert(target_ids, with_delete, kWriteTwice);
    VerifyRead(target_ids, expect_success, is_updated);
    VerifyScan(begin_ref, end_ref, expect_success, is_updated);

    DestroyData();
  }

  void
  VerifyUpdatesWith(  //
      const bool with_write,
      const bool with_delete,
      const AccessPattern pattern)
  {
    if (!HasUpdateOperation<ImplStat>()  //
        || (with_write && !HasWriteOperation<ImplStat>())
        || (with_delete && !HasDeleteOperation<ImplStat>()))  //
    {
      GTEST_SKIP();
    }

    PrepareData();

    const auto &target_ids = CreateTargetIDs(kExecNum, pattern);
    const auto &begin_ref = std::make_pair(0, kRangeClosed);
    const auto &end_ref = std::make_pair(kExecNum, kRangeOpened);
    const auto expect_update = with_write && !with_delete;

    if (with_write) VerifyWrite(target_ids);
    if (with_delete) VerifyDelete(target_ids, with_write);
    VerifyUpdate(target_ids, expect_update);
    VerifyRead(target_ids, expect_update, kWriteTwice);
    VerifyScan(begin_ref, end_ref, expect_update, kWriteTwice);

    DestroyData();
  }

  void
  VerifyDeletesWith(  //
      const bool with_write,
      const bool with_delete,
      const AccessPattern pattern)
  {
    if (!HasDeleteOperation<ImplStat>()                     //
        || (with_write && !HasWriteOperation<ImplStat>()))  //
    {
      GTEST_SKIP();
    }

    PrepareData();

    const auto &target_ids = CreateTargetIDs(kExecNum, pattern);
    const auto &begin_ref = std::make_pair(0, kRangeClosed);
    const auto &end_ref = std::make_pair(kExecNum, kRangeOpened);
    const auto expect_delete = with_write && !with_delete;

    if (with_write) VerifyWrite(target_ids);
    if (with_delete) VerifyDelete(target_ids, with_write);
    VerifyDelete(target_ids, expect_delete);
    VerifyRead(target_ids, kExpectFailed);
    VerifyScan(begin_ref, end_ref, kExpectFailed);

    DestroyData();
  }

  void
  VerifyBulkloadWith(  //
      const WriteOperation write_ops,
      const AccessPattern pattern)
  {
    if (!HasBulkloadOperation<ImplStat>()                              //
        || (write_ops == kWrite && !HasWriteOperation<ImplStat>())     //
        || (write_ops == kInsert && !HasInsertOperation<ImplStat>())   //
        || (write_ops == kUpdate && !HasUpdateOperation<ImplStat>())   //
        || (write_ops == kDelete && !HasDeleteOperation<ImplStat>()))  //
    {
      GTEST_SKIP();
    }

    PrepareData();

    const auto &target_ids = CreateTargetIDs(kExecNum, pattern);
    const auto &begin_ref = std::make_pair(0, kRangeClosed);
    const auto &end_ref = std::make_pair(kExecNum, kRangeOpened);
    auto expect_success = true;
    auto is_updated = false;

    VerifyBulkload();
    switch (write_ops) {
      case kWrite:
        VerifyWrite(target_ids, kWriteTwice);
        is_updated = true;
        break;
      case kInsert:
        VerifyInsert(target_ids, kExpectFailed);
        break;
      case kUpdate:
        VerifyUpdate(target_ids, kExpectSuccess);
        is_updated = true;
        break;
      case kDelete:
        VerifyDelete(target_ids, kExpectSuccess);
        expect_success = false;
        break;
      case kWithoutWrite:
      default:
        break;
    }
    VerifyRead(target_ids, expect_success, is_updated);
    VerifyScan(begin_ref, end_ref, expect_success, is_updated);

    DestroyData();
  }

  /*####################################################################################
   * Internal member variables
   *##################################################################################*/

  /// actual keys
  std::vector<Key> keys_{};

  /// actual payloads
  std::vector<Payload> payloads_{};

  /// an index for testing
  std::unique_ptr<Index_t> index_{nullptr};

  // an epoch manager for multi-version
  std::shared_ptr<EpochManager> epoch_manager_{nullptr};
};

}  // namespace dbgroup::index::test

#endif  // INDEX_FIXTURES_INDEX_FIXTURE_HPP
