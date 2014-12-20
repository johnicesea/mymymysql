#include "context/context.hpp"
#include <iostream>
#include <algorithm>
#include <ctime>
#include <cstdio>
#include <set>

void test(PageDB::Scheduler* pgdb) {
    std::srand((unsigned int)time(NULL));
    std::remove("test.db");
    std::remove("test:wdy.db");
    std::remove("test:wdy.idx");
    Context::Context ctx(pgdb);
    TypeDB::TableDesc desc;
    TypeDB::ColDesc col_desc1, col_desc2, col_desc3;
    col_desc1.type = TypeDB::intType;
    col_desc2.type = TypeDB::stringType;
    col_desc3.type = TypeDB::stringType;
    col_desc1.name = "W";
    col_desc2.name = "D";
    col_desc3.name = "Y";
    desc.descs.push_back(col_desc1);
    desc.descs.push_back(col_desc2);
    desc.descs.push_back(col_desc3);
    desc.primaryIndex = 0;
    ctx.InitTable("wdy", desc);
    TypeDB::Table tbl;
    tbl.desc = desc;
    TypeDB::Row row1, row2, row3;
    row1.desc = row2.desc = row3.desc = &tbl.desc;
    row1.objs.push_back(new TypeDB::Int(1));
    row1.objs.push_back(new TypeDB::String("wdy"));
    row1.objs.push_back(new TypeDB::String("abc"));
    row2.objs.push_back(new TypeDB::Int(2));
    row2.objs.push_back(new TypeDB::String("wy"));
    row2.objs.push_back(new TypeDB::String("lo"));
    row3.objs.push_back(new TypeDB::Int(3));
    row3.objs.push_back(new TypeDB::String("XYZ"));
    row3.objs.push_back(new TypeDB::String("ve"));
    tbl.rows.push_back(row1);
    tbl.rows.push_back(row2);
    tbl.rows.push_back(row3);
    ctx.Insert("wdy", tbl);
    TypeDB::Table tbl_rst = ctx.GetTable("wdy");
    for (auto row : tbl_rst.rows) {
        for (auto item : row.objs)
            std::cout << item->toString() << " ";
        std::cout << std::endl;
    }
}

int main() {
    PageDB::Scheduler* pgdb = new PageDB::LRU_Scheduler;
    pgdb->StartSchedule();
    test(pgdb);
    pgdb->StopSchedule();
    delete pgdb;
}
