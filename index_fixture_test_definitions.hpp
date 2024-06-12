/*--------------------------------------------------------------------------------------
 * Structure modification operations
 *------------------------------------------------------------------------------------*/

TYPED_TEST(IndexFixture, ConstructWithoutSMOs)
{
  constexpr auto kRecNum = TestFixture::kRecNumWithoutSMOs;
  TestFixture::VerifyWritesWith(!kWriteTwice, !kWithDelete, kSequential, kRecNum);
}

TYPED_TEST(IndexFixture, ConstructWithLeafSMOs)
{
  constexpr auto kRecNum = TestFixture::kRecNumWithLeafSMOs;
  TestFixture::VerifyWritesWith(!kWriteTwice, !kWithDelete, kSequential, kRecNum);
}

TYPED_TEST(IndexFixture, ConstructWithInternalSMOs)
{
  constexpr auto kRecNum = TestFixture::kRecNumWithInternalSMOs;
  TestFixture::VerifyWritesWith(!kWriteTwice, !kWithDelete, kSequential, kRecNum);
}

/*--------------------------------------------------------------------------------------
 * SnapshotRead operation
 *------------------------------------------------------------------------------------*/

// TYPED_TEST(IndexFixture, SnapshotRead)
// {
//   TestFixture::FillIndex();
//   TestFixture::VerifySnapshotRead();
// }

/*--------------------------------------------------------------------------------------
 * Read operation tests
 *------------------------------------------------------------------------------------*/

TYPED_TEST(IndexFixture, ReadWithEmptyIndexFail)
{  //
  TestFixture::VerifyReadEmptyIndex();
}

/*--------------------------------------------------------------------------------------
 * Scan operation
 *------------------------------------------------------------------------------------*/

TYPED_TEST(IndexFixture, ScanWithoutKeysPerformFullScan)
{
  TestFixture::VerifyScanWith(!kHasRange);
}

TYPED_TEST(IndexFixture, ScanWithClosedRangeIncludeLeftRightEnd)
{
  TestFixture::VerifyScanWith(kHasRange, kRangeClosed);
}

TYPED_TEST(IndexFixture, ScanWithOpenedRangeExcludeLeftRightEnd)
{
  TestFixture::VerifyScanWith(kHasRange, kRangeOpened);
}

/*--------------------------------------------------------------------------------------
 * Write operation
 *------------------------------------------------------------------------------------*/

TYPED_TEST(IndexFixture, SequentialWriteWithUniqueKeysSucceed)
{
  TestFixture::VerifyWritesWith(!kWriteTwice, !kWithDelete, kSequential);
}

TYPED_TEST(IndexFixture, SequentialWriteWithDuplicateKeysSucceed)
{
  TestFixture::VerifyWritesWith(kWriteTwice, !kWithDelete, kSequential);
}

/* delete is not implemented yet*/
// TYPED_TEST(IndexFixture, SequentialWriteWithDeletedKeysSucceed)
// {
//   TestFixture::VerifyWritesWith(kWriteTwice, kWithDelete, kSequential);
// }

TYPED_TEST(IndexFixture, ReverseWriteWithUniqueKeysSucceed)
{
  TestFixture::VerifyWritesWith(!kWriteTwice, !kWithDelete, kReverse);
}

TYPED_TEST(IndexFixture, ReverseWriteWithDuplicateKeysSucceed)
{
  TestFixture::VerifyWritesWith(kWriteTwice, !kWithDelete, kReverse);
}

// TYPED_TEST(IndexFixture, ReverseWriteWithDeletedKeysSucceed)
// {
//   TestFixture::VerifyWritesWith(kWriteTwice, kWithDelete, kReverse);
// }

TYPED_TEST(IndexFixture, RandomWriteWithUniqueKeysSucceed)
{
  TestFixture::VerifyWritesWith(!kWriteTwice, !kWithDelete, kRandom);
}

TYPED_TEST(IndexFixture, RandomWriteWithDuplicateKeysSucceed)
{
  TestFixture::VerifyWritesWith(kWriteTwice, !kWithDelete, kRandom);
}

// TYPED_TEST(IndexFixture, RandomWriteWithDeletedKeysSucceed)
// {
//   TestFixture::VerifyWritesWith(kWriteTwice, kWithDelete, kRandom);
// }

/*--------------------------------------------------------------------------------------
 * Insert operation
 *------------------------------------------------------------------------------------*/

/* insert is not implemented yet*/
// TYPED_TEST(IndexFixture, SequentialInsertWithUniqueKeysSucceed)
// {
//   TestFixture::VerifyInsertsWith(!kWriteTwice, !kWithDelete, kSequential);
// }

// TYPED_TEST(IndexFixture, SequentialInsertWithDuplicateKeysFail)
// {
//   TestFixture::VerifyInsertsWith(kWriteTwice, !kWithDelete, kSequential);
// }

// TYPED_TEST(IndexFixture, SequentialInsertWithDeletedKeysSucceed)
// {
//   TestFixture::VerifyInsertsWith(kWriteTwice, kWithDelete, kSequential);
// }

// TYPED_TEST(IndexFixture, ReverseInsertWithUniqueKeysSucceed)
// {
//   TestFixture::VerifyInsertsWith(!kWriteTwice, !kWithDelete, kReverse);
// }

// TYPED_TEST(IndexFixture, ReverseInsertWithDuplicateKeysFail)
// {
//   TestFixture::VerifyInsertsWith(kWriteTwice, !kWithDelete, kReverse);
// }

// TYPED_TEST(IndexFixture, ReverseInsertWithDeletedKeysSucceed)
// {
//   TestFixture::VerifyInsertsWith(kWriteTwice, kWithDelete, kReverse);
// }

// TYPED_TEST(IndexFixture, RandomInsertWithUniqueKeysSucceed)
// {
//   TestFixture::VerifyInsertsWith(!kWriteTwice, !kWithDelete, kRandom);
// }

// TYPED_TEST(IndexFixture, RandomInsertWithDuplicateKeysFail)
// {
//   TestFixture::VerifyInsertsWith(kWriteTwice, !kWithDelete, kRandom);
// }

// TYPED_TEST(IndexFixture, RandomInsertWithDeletedKeysSucceed)
// {
//   TestFixture::VerifyInsertsWith(kWriteTwice, kWithDelete, kRandom);
// }

/*--------------------------------------------------------------------------------------
 * Update operation
 *------------------------------------------------------------------------------------*/

/* update is not implemented yet*/
// TYPED_TEST(IndexFixture, SequentialUpdateWithDuplicateKeysSucceed)
// {
//   TestFixture::VerifyUpdatesWith(kWithWrite, !kWithDelete, kSequential);
// }

// TYPED_TEST(IndexFixture, SequentialUpdateWithNotInsertedKeysFail)
// {
//   TestFixture::VerifyUpdatesWith(!kWithWrite, !kWithDelete, kSequential);
// }

// TYPED_TEST(IndexFixture, SequentialUpdateWithDeletedKeysFail)
// {
//   TestFixture::VerifyUpdatesWith(kWithWrite, kWithDelete, kSequential);
// }

// TYPED_TEST(IndexFixture, ReverseUpdateWithDuplicateKeysSucceed)
// {
//   TestFixture::VerifyUpdatesWith(kWithWrite, !kWithDelete, kReverse);
// }

// TYPED_TEST(IndexFixture, ReverseUpdateWithNotInsertedKeysFail)
// {
//   TestFixture::VerifyUpdatesWith(!kWithWrite, !kWithDelete, kReverse);
// }

// TYPED_TEST(IndexFixture, ReverseUpdateWithDeletedKeysFail)
// {
//   TestFixture::VerifyUpdatesWith(kWithWrite, kWithDelete, kReverse);
// }

// TYPED_TEST(IndexFixture, RandomUpdateWithDuplicateKeysSucceed)
// {
//   TestFixture::VerifyUpdatesWith(kWithWrite, !kWithDelete, kRandom);
// }

// TYPED_TEST(IndexFixture, RandomUpdateWithNotInsertedKeysFail)
// {
//   TestFixture::VerifyUpdatesWith(!kWithWrite, !kWithDelete, kRandom);
// }

// TYPED_TEST(IndexFixture, RandomUpdateWithDeletedKeysFail)
// {
//   TestFixture::VerifyUpdatesWith(kWithWrite, kWithDelete, kRandom);
// }

/*--------------------------------------------------------------------------------------
 * Delete operation
 *------------------------------------------------------------------------------------*/

/* delete is not implemented yet*/
// TYPED_TEST(IndexFixture, SequentialDeleteWithDuplicateKeysSucceed)
// {
//   TestFixture::VerifyDeletesWith(kWithWrite, !kWithDelete, kSequential);
// }

// TYPED_TEST(IndexFixture, SequentialDeleteWithNotInsertedKeysFail)
// {
//   TestFixture::VerifyDeletesWith(!kWithWrite, !kWithDelete, kSequential);
// }

// TYPED_TEST(IndexFixture, SequentialDeleteWithDeletedKeysFail)
// {
//   TestFixture::VerifyDeletesWith(kWithWrite, kWithDelete, kSequential);
// }

// TYPED_TEST(IndexFixture, ReverseDeleteWithDuplicateKeysSucceed)
// {
//   TestFixture::VerifyDeletesWith(kWithWrite, !kWithDelete, kReverse);
// }

// TYPED_TEST(IndexFixture, ReverseDeleteWithNotInsertedKeysFail)
// {
//   TestFixture::VerifyDeletesWith(!kWithWrite, !kWithDelete, kReverse);
// }

// TYPED_TEST(IndexFixture, ReverseDeleteWithDeletedKeysFail)
// {
//   TestFixture::VerifyDeletesWith(kWithWrite, kWithDelete, kReverse);
// }

// TYPED_TEST(IndexFixture, RandomDeleteWithDuplicateKeysSucceed)
// {
//   TestFixture::VerifyDeletesWith(kWithWrite, !kWithDelete, kRandom);
// }

// TYPED_TEST(IndexFixture, RandomDeleteWithNotInsertedKeysFail)
// {
//   TestFixture::VerifyDeletesWith(!kWithWrite, !kWithDelete, kRandom);
// }

// TYPED_TEST(IndexFixture, RandomDeleteWithDeletedKeysFail)
// {
//   TestFixture::VerifyDeletesWith(kWithWrite, kWithDelete, kRandom);
// }

/*--------------------------------------------------------------------------------------
 * Bulkload operation
 *------------------------------------------------------------------------------------*/

TYPED_TEST(IndexFixture, BulkloadWithoutAdditionalWriteOperations)
{
  TestFixture::VerifyBulkloadWith(kWithoutWrite, kSequential);
}

TYPED_TEST(IndexFixture, BulkloadWithSequentialWrite)
{
  TestFixture::VerifyBulkloadWith(kWrite, kSequential);
}

// TYPED_TEST(IndexFixture, BulkloadWithSequentialInsert)
// {
//   TestFixture::VerifyBulkloadWith(kInsert, kSequential);
// }

// TYPED_TEST(IndexFixture, BulkloadWithSequentialUpdate)
// {
//   TestFixture::VerifyBulkloadWith(kUpdate, kSequential);
// }

// TYPED_TEST(IndexFixture, BulkloadWithSequentialDelete)
// {
//   TestFixture::VerifyBulkloadWith(kDelete, kSequential);
// }

TYPED_TEST(IndexFixture, BulkloadWithReverseWrite)
{
  TestFixture::VerifyBulkloadWith(kWrite, kReverse);
}

// TYPED_TEST(IndexFixture, BulkloadWithReverseInsert)
// {
//   TestFixture::VerifyBulkloadWith(kInsert, kReverse);
// }

// TYPED_TEST(IndexFixture, BulkloadWithReverseUpdate)
// {
//   TestFixture::VerifyBulkloadWith(kUpdate, kReverse);
// }

// TYPED_TEST(IndexFixture, BulkloadWithReverseDelete)
// {
//   TestFixture::VerifyBulkloadWith(kDelete, kReverse);
// }

TYPED_TEST(IndexFixture, BulkloadWithRandomWrite)
{
  TestFixture::VerifyBulkloadWith(kWrite, kRandom);
}

// TYPED_TEST(IndexFixture, BulkloadWithRandomInsert)
// {
//   TestFixture::VerifyBulkloadWith(kInsert, kRandom);
// }

// TYPED_TEST(IndexFixture, BulkloadWithRandomUpdate)
// {
//   TestFixture::VerifyBulkloadWith(kUpdate, kRandom);
// }

// TYPED_TEST(IndexFixture, BulkloadWithRandomDelete)
// {
//   TestFixture::VerifyBulkloadWith(kDelete, kRandom);
// }
