//https://www.jianshu.com/p/573c5706da0a
#include <iostream>
#include <cassert>
#include <leveldb/db.h>
#include <leveldb/write_batch.h>

int main()
{
	//打开一个数据库
	leveldb::DB* db;
	leveldb::Options opts;
	opts.create_if_missing = true;
	leveldb::Status status = leveldb::DB::Open(opts, "./testdb2",&db);
	assert(status.ok());

	//写入数据
	status = db->Put(leveldb::WriteOptions(), "key0", "value0");
	assert(status.ok());

	//读出数据
	std::string val;
	status = db->Get(leveldb::ReadOptions(), "key0", &val);
	assert(status.ok());
	std::cout << val << "\n";

	//批量原子写入
	leveldb::WriteBatch batch;
	batch.Delete("key0");
	std::string tmp_key = "key0";
	std::string tmp_val = "value0";
	for(int i = 0;i < 10;i++)
	{
		tmp_key[3] = i+'0';
		tmp_val[5] = i+'0';
		batch.Put(tmp_key, tmp_val);
	}
	status = db->Write(leveldb::WriteOptions(), &batch);
	assert(status.ok());

	//扫描数据库
	leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
	for(it->SeekToFirst(); it->Valid(); it->Next())
	{
		std::cout << it->key().ToString() << " : " << it->value().ToString() << std::endl;
	}
	assert(it->status().ok());

	//范围查找
	for(it->Seek("key3");it->Valid()&&it->key().ToString()<"key8";it->Next())
	{
		std::cout << it->key().ToString() << " : " << it->value().ToString() << std::endl;
	}

	//删除db
	delete db;

}
