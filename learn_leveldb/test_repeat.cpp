//测试插入重复的key值会发生什么
#include <leveldb/db.h>
#include <leveldb/write_batch.h>
#include <iostream>

using namespace std;

int main()
{
	//打开数据库
	leveldb::DB* db;
	leveldb::Options opts;
	opts.create_if_missing = true;
	leveldb::Status s = leveldb::DB::Open(opts, "./testdb2", &db);
	assert(s.ok());

	//测试插入重复的数据
	s = db->Put(leveldb::WriteOptions(), "key2", "new_value");
	assert(s.ok());

	//读取数据
	string val;
	s = db->Get(leveldb::ReadOptions(), "key2", &val);
	assert(s.ok());
	cout << "val = " << val << endl;
}
