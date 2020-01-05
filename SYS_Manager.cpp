#include "stdafx.h"
#include "EditArea.h"
#include "SYS_Manager.h"
#include "QU_Manager.h"
#include "HustBase.h"
#include "HustBaseDoc.h"
#include <iostream>
#include <vector>
#include <set>
#include <string>
using std::set;
using std::string;
bool isOpened = false;//标识数据库是否打开

/*自定义函数*/
//将字符串转换为小写
void toLowerString(std::string &s);
//在界面显示一条消息
void showMsg(CEditArea *editArea, char * msg);
//输出select的查询结果
void showSelectResult(SelResult &res, CEditArea *editArea);


void ExecuteAndMessage(char * sql, CEditArea* editArea) {//根据执行的语句类型在界面上显示执行结果。此函数需修改
	std::string s_sql = sql;
	RC rc;
	toLowerString(s_sql);
	//查询语句的处理
	if (s_sql.find("select") == 0) {
		SelResult res;
		Init_Result(&res);
		rc = Query(sql, &res);
		if (rc == SUCCESS) {
			showSelectResult(res, editArea);
			return;
		}
	}
	//其他语句的处理
	else {
		//执行
		rc = execute(sql);

		//更新界面
		CHustBaseDoc *pDoc;
		pDoc = CHustBaseDoc::GetDoc();
		CHustBaseApp::pathvalue = true;
		pDoc->m_pTreeView->PopulateTree();
	}

	//执行结果
	switch (rc) {
	case SUCCESS:
		showMsg(editArea, "操作成功");
		break;
	case SQL_SYNTAX:
		showMsg(editArea, "有语法错误");
		break;
	case TABLE_NOT_EXIST:
		showMsg(editArea, "表不存在");
		break;
	case PF_FILEERR:
		showMsg(editArea, "文件不存在");
		break;
	case TABLE_EXIST:
		showMsg(editArea, "表已存在，无法重复创建");
		break;
	case TABLE_COLUMN_ERROR:
		showMsg(editArea, "属性名过长或属性同名");
		break;
	case DB_NOT_EXIST:
		showMsg(editArea, "数据库不存在");
		break;
	default:
		showMsg(editArea, "错误");
		break;
	}
}

RC execute(char * sql) {
	sqlstr *sql_str = NULL;
	RC rc;
	sql_str = get_sqlstr();
	rc = parse(sql, sql_str);//只有两种返回结果SUCCESS和SQL_SYNTAX

	if (rc != SUCCESS)
		return rc;

	switch (sql_str->flag)
	{
		//case 1:
		////判断SQL语句为select语句

		//break;

	case 2:
		//判断SQL语句为insert语句
		rc = Insert(sql_str->sstr.ins.relName, sql_str->sstr.ins.nValues, sql_str->sstr.ins.values);
		break;
	case 3:
		//判断SQL语句为update语句
		rc = Update(sql_str->sstr.upd.relName, sql_str->sstr.upd.attrName, &(sql_str->sstr.upd.value), sql_str->sstr.upd.nConditions, sql_str->sstr.upd.conditions);
		break;

	case 4:
		//判断SQL语句为delete语句
		rc = Delete(sql_str->sstr.del.relName, sql_str->sstr.del.nConditions, sql_str->sstr.del.conditions);
		break;

	case 5:
		//判断SQL语句为createTable语句
		rc = CreateTable(sql_str->sstr.cret.relName, sql_str->sstr.cret.attrCount, sql_str->sstr.cret.attributes);
		break;

	case 6:
		//判断SQL语句为dropTable语句
		rc = DropTable(sql_str->sstr.drt.relName);
		break;

	case 7:
		//判断SQL语句为createIndex语句
		rc = CreateIndex(sql_str->sstr.crei.indexName, sql_str->sstr.crei.relName, sql_str->sstr.crei.attrName);
		break;

	case 8:
		//判断SQL语句为dropIndex语句
		rc = DropIndex(sql_str->sstr.dri.indexName);
		break;

	case 9:
		//判断为help语句，可以给出帮助提示
		break;

	case 10:
		//判断为exit语句，可以由此进行退出操作
		break;
	}
	return rc;
}

RC CreateDB(char *dbpath, char *dbname) {
	//在dbpath下创建dbname文件夹
	CString path = dbpath;
	path += "\\";
	path += dbname;
	CreateDirectory(path, NULL);

	//在dbname文件夹下创建表文件和列文件
	SetCurrentDirectory(path);
	RC rc;
	rc = RM_CreateFile("SYSTABLES", sizeof(SysTable));
	if (rc != SUCCESS)
		return rc;
	rc = RM_CreateFile("SYSCOLUMNS", sizeof(SysColumn));
	if (rc != SUCCESS)
		return rc;
	return SUCCESS;
}

RC DropDB(char *dbname) {
	SetCurrentDirectory(dbname);
	if (_access("SYSTABLES", 0) == -1 || _access("SYSCOLUMNS", 0) == -1) {
		return DB_NOT_EXIST;
	}
	BOOL ret = TRUE;
	CFileFind finder;
	CString path;
	path.Format(_T("%s/*"), dbname);
	BOOL bWorking = finder.FindFile(path);
	while (bWorking)
	{
		bWorking = finder.FindNextFile();
		if (finder.IsDirectory() && !finder.IsDots())
		{//处理文件夹
			char subDir[200];
			std::string dir = finder.GetFilePath();
			sprintf_s(subDir, "%s", dir.c_str());
			DropDB(subDir); //递归删除文件夹
			RemoveDirectory(finder.GetFilePath());
		}
		else
		{//处理文件
			DeleteFile(finder.GetFilePath());
		}
	}
	return SUCCESS;
}

RC OpenDB(char *dbname) {
	SetCurrentDirectory(dbname);
	if (_access("SYSTABLES", 0) == -1 || _access("SYSCOLUMNS", 0) == -1) {
		return DB_NOT_EXIST;
	}
	isOpened = true;
	return SUCCESS;
}

RC CloseDB() {
	isOpened = false;
	return SUCCESS;
}


RC CreateTable(char *relName, int attrCount, AttrInfo *attributes) {
	RC rc;
	char  *pData;
	RM_FileHandle rm_table, rm_column;
	RID rid;
	int recordsize;//数据表的每条记录的大小
	AttrInfo * attrtmp = attributes;

	//表存在
	if (_access(relName, 0) == 0)
		return TABLE_EXIST;

	//判断表名是否过长
	if (strlen(relName) > 20)
		return TABLE_NAME_ILLEGAL;

	//判断属性名是否符合要求
	set<string> s;
	for (int i = 0; i < attrCount; i++) {
		if (s.find(attributes[i].attrName) != s.end())//存在同名属性
			return TABLE_COLUMN_ERROR;
		s.insert(attributes[i].attrName);
		if (strlen(attributes[i].attrName) > 10)
			return TABLE_COLUMN_ERROR;
	}
	/*打开系统表文件并更新*/
	rm_table.bOpen = false;
	rc = RM_OpenFile("SYSTABLES", &rm_table);
	if (rc != SUCCESS)
		return rc;

	pData = new char[sizeof(SysTable)];
	memcpy(pData, relName, 21);
	memcpy(pData + 21, &attrCount, sizeof(int));
	rid.bValid = false;
	rc = InsertRec(&rm_table, pData, &rid);
	if (rc != SUCCESS) {
		return rc;
	}
	rc = RM_CloseFile(&rm_table);
	if (rc != SUCCESS) {
		return rc;
	}
	//释放申请的内存空间
	delete[] pData;

	/*打开列文件并更新*/
	rm_column.bOpen = false;
	rc = RM_OpenFile("SYSCOLUMNS", &rm_column);
	if (rc != SUCCESS)
		return rc;
	for (int i = 0, offset = 0; i < attrCount; i++, attrtmp++) {
		pData = new char[sizeof(SysColumn)];
		memcpy(pData, relName, 21);
		memcpy(pData + 21, attrtmp->attrName, 21);
		memcpy(pData + 42, &(attrtmp->attrType), sizeof(AttrType));
		memcpy(pData + 42 + sizeof(AttrType), &(attrtmp->attrLength), sizeof(int));
		memcpy(pData + 42 + sizeof(int) + sizeof(AttrType), &offset, sizeof(int));
		memcpy(pData + 42 + 2 * sizeof(int) + sizeof(AttrType), "0", sizeof(char));
		rid.bValid = false;
		rc = InsertRec(&rm_column, pData, &rid);
		if (rc != SUCCESS) {
			return rc;
		}
		delete[] pData;
		offset += attrtmp->attrLength;
	}
	rc = RM_CloseFile(&rm_column);
	if (rc != SUCCESS) {
		return rc;
	}

	/*创建数据表*/

	//计算recordsize的大小
	recordsize = 0;
	attrtmp = attributes;
	for (int i = 0; i < attrCount; i++, attrtmp++) {
		recordsize += attrtmp->attrLength;
	}
	rc = RM_CreateFile(relName, recordsize);
	if (rc != SUCCESS)
		return rc;
	return SUCCESS;
}

RC DropTable(char *relName) {
	CFile tmp;
	RM_FileHandle rm_table, rm_column;
	RC rc;
	RM_FileScan FileScan;
	RM_Record rectab, reccol;
	int attrcount;//临时 属性数量，属性长度，属性偏移

	//删除不不存在的表
	if (_access(relName, 0) == -1)
		return TABLE_NOT_EXIST;

	/*删除数据表文件*/
	tmp.Remove((LPCTSTR)relName);

	/*将系统表和系统列中对应该数据表的相关记录删除*/
	rm_table.bOpen = false;
	rc = RM_OpenFile("SYSTABLES", &rm_table);
	if (rc != SUCCESS)
		return rc;
	rm_column.bOpen = false;
	rc = RM_OpenFile("SYSCOLUMNS", &rm_column);
	if (rc != SUCCESS)
		return rc;

	//获取系统表信息
	FileScan.bOpen = false;
	rc = OpenScan(&FileScan, &rm_table, 0, NULL);
	if (rc != SUCCESS)
		return rc;

	while (GetNextRec(&FileScan, &rectab) == SUCCESS) {//循环遍历记录
		if (strcmp(relName, rectab.pData) == 0) {
			memcpy(&attrcount, rectab.pData + 21, sizeof(int));
			DeleteRec(&rm_table, &(rectab.rid));//调用记录管理模块接口删除该表相关信息
			delete[] rectab.pData;
			break;
		}
		delete[] rectab.pData;
	}

	//获取系统列信息
	FileScan.bOpen = false;
	rc = OpenScan(&FileScan, &rm_column, 0, NULL);
	if (rc != SUCCESS)
		return rc;
	//根据之前读取的系统表中信息，读取属性信息
	for (int i = 0; i < attrcount && GetNextRec(&FileScan, &reccol) == SUCCESS;) {
		if (strcmp(relName, reccol.pData) == 0) {//删除属性记录
			DeleteRec(&rm_column, &(reccol.rid));
			i++;
		}
		delete[] reccol.pData;
	}

	//关闭文件句柄
	rc = RM_CloseFile(&rm_table);
	if (rc != SUCCESS)
		return rc;
	rc = RM_CloseFile(&rm_column);
	if (rc != SUCCESS)
		return rc;
	return SUCCESS;
}

RC CreateIndex(char *indexName, char *relName, char *attrName) {
	//修改属性信息，设置索引字段为存在索引
	RC rc;

	RM_Record colRec;

	//打开系统列文件
	RM_FileHandle colHandle;
	colHandle.bOpen = false;
	rc = RM_OpenFile("SYSCOLUMNS", &colHandle);
	CHECK_RC(rc);

	//设置扫描条件
	Con conditions[2];
	conditions[0].attrType = chars;
	conditions[0].compOp = EQual;
	conditions[0].bLhsIsAttr = 1;
	conditions[0].LattrLength = 21;
	conditions[0].LattrOffset = 0;
	conditions[0].bRhsIsAttr = 0;
	conditions[0].Rvalue = relName;

	conditions[1].attrType = chars;
	conditions[1].compOp = EQual;
	conditions[1].bLhsIsAttr = 1;
	conditions[1].LattrLength = 21;
	conditions[1].LattrOffset = 21;
	conditions[1].bRhsIsAttr = 0;
	conditions[1].Rvalue = attrName;
	RM_FileScan fileScan;
	fileScan.bOpen = false;
	OpenScan(&fileScan, &colHandle, 2, conditions);

	rc = GetNextRec(&fileScan, &colRec);
	if (rc != SUCCESS)
	{
		return rc;
	}

	if (*(colRec.pData + 42 + 3 * sizeof(int)) != '0')
	{
		return FAIL;
	}

	*(colRec.pData + 42 + 3 * sizeof(int)) = '1';   //设置索引标识为1
	memset(colRec.pData + 42 + 3 * sizeof(int) + sizeof(char), '\0', 21);
	memcpy(colRec.pData + 42 + 3 * sizeof(int) + sizeof(char), indexName, strlen(indexName));

	//更新系统列文件
	UpdateRec(&colHandle, &colRec);

	//关闭扫描及系统列文件
	CloseScan(&fileScan);
	RM_CloseFile(&colHandle);

	//创建并打开索引文件
	AttrType attrType;
	memcpy(&attrType, colRec.pData + 42, sizeof(int));

	//创建索引文件
	int length;
	memcpy(&length, colRec.pData + 42 + sizeof(int), sizeof(int));
	CreateIndex(indexName, attrType, length);

	//打开索引文件
	IX_IndexHandle indexHandle;
	indexHandle.bOpen = false;
	OpenIndex(indexName, &indexHandle);

	//向索引文件中加入索引项
	//首先，打开记录文件,扫描所有记录
	RM_FileHandle recFileHandle;
	RM_FileScan recFileScan;
	RM_Record rec;

	recFileHandle.bOpen = false;
	recFileScan.bOpen = false;
	rec.bValid = false;

	RM_OpenFile(relName, &recFileHandle);
	OpenScan(&recFileScan, &recFileHandle, 0, NULL);

	int attrOffset, attrLength;
	memcpy(&attrOffset, colRec.pData + 42 + 2 * sizeof(int), sizeof(int));
	memcpy(&attrLength, colRec.pData + 42 + sizeof(int), sizeof(int));

	char *attrValue = NULL;
	attrValue = (char *)malloc(sizeof(char)*attrLength);

	//将扫描到的记录插入到索引中
	while (GetNextRec(&recFileScan, &rec) == SUCCESS)
	{
		memcpy(attrValue, rec.pData + attrOffset, attrLength);
		InsertEntry(&indexHandle, attrValue, &rec.rid);
	}

	CloseScan(&recFileScan);
	RM_CloseFile(&recFileHandle);
	CloseIndex(&indexHandle);

	return SUCCESS;
}

RC DropIndex(char *indexName) {
	IX_IndexHandle indexHandle;
	RC rc;

	//判断索引是否存在
	indexHandle.bOpen = false;
	if ((rc = OpenIndex(indexName, &indexHandle)) != SUCCESS)
	{
		return rc;
	}
	CloseIndex(&indexHandle);

	//从表属性中清除索引标识
	RM_FileHandle colHandle;
	RM_FileScan fileScan;
	RM_Record colRec;

	colHandle.bOpen = false;
	fileScan.bOpen = false;

	//打开系统列文件
	rc = RM_OpenFile("SYSCOLUMNS", &colHandle);
	if (rc != SUCCESS)
	{
		return rc;
	}

	//设置扫描条件
	Con *conditions = NULL;
	conditions = (Con *)malloc(sizeof(Con));
	(*conditions).attrType = chars;
	(*conditions).compOp = EQual;
	(*conditions).bLhsIsAttr = 1;
	(*conditions).LattrLength = strlen(indexName) + 1;
	(*conditions).LattrOffset = 42 + 3 * sizeof(int) + sizeof(char);
	(*conditions).bRhsIsAttr = 0;
	(*conditions).Rvalue = indexName;

	OpenScan(&fileScan, &colHandle, 1, conditions);

	rc = GetNextRec(&fileScan, &colRec);

	if (rc == SUCCESS) {
		*(colRec.pData + 42 + 3 * sizeof(int)) = '0';   //设置索引标识为0
		memset(colRec.pData + 42 + 3 * sizeof(int) + sizeof(char), '\0', 21);
		//更新系统列文件
		UpdateRec(&colHandle, &colRec);
	}

	//关闭扫描及系统列文件
	CloseScan(&fileScan);
	RM_CloseFile(&colHandle);

	DeleteFile((LPCTSTR)indexName);//删除数据表文件

	return SUCCESS;
}

RC Insert(char *relName, int nValues, Value *values) {
	RC rc;
	/*判断表是否存在*/
	if (_access(relName, 0) == -1)
		return TABLE_NOT_EXIST;

	/*扫描系统表获得表的属性个数*/
	RM_FileHandle rm_table;
	rm_table.bOpen = false;
	rc = RM_OpenFile("SYSTABLES", &rm_table);
	if (rc != SUCCESS)
		return rc;

	Con con;
	con.attrType = chars;
	con.bLhsIsAttr = 1;
	con.LattrOffset = 0;
	con.LattrLength = strlen(relName) + 1;
	con.compOp = EQual;
	con.bRhsIsAttr = 0;
	con.Rvalue = relName;

	RM_FileScan rm_fileScan;
	RM_Record record;
	rm_fileScan.bOpen = false;
	rc = OpenScan(&rm_fileScan, &rm_table, 1, &con);
	if (rc != SUCCESS)
		return rc;
	rc = GetNextRec(&rm_fileScan, &record);
	if (rc != SUCCESS)
		return rc;

	int attrcount = 0;
	memcpy(&attrcount, record.pData + 21, sizeof(int));
	CloseScan(&rm_fileScan);
	RM_CloseFile(&rm_table);

	if (attrcount != nValues) {//列数与表定义不一致
		return FIELD_MISSING;
	}

	/*获取系统列信息*/
	RM_FileHandle rm_column;
	rm_column.bOpen = false;
	rc = RM_OpenFile("SYSCOLUMNS", &rm_column);
	if (rc != SUCCESS) {
		return rc;
	}
	rc = OpenScan(&rm_fileScan, &rm_column, 0, NULL);
	if (rc != SUCCESS) {
		return rc;
	}
	SysColumn * column;
	column = new SysColumn[attrcount]();
	RM_Record reccol;
	while (GetNextRec(&rm_fileScan, &reccol) == SUCCESS) {
		if (strcmp(relName, reccol.pData) == 0) {//如果找到为relName的记录，然后读取属性
			for (int i = 0; i < attrcount; i++) {
				memcpy(column[i].tabName, reccol.pData, 21);
				memcpy(column[i].attrName, reccol.pData + 21, 21);
				memcpy(&(column[i].attrType), reccol.pData + 42, sizeof(AttrType));
				memcpy(&(column[i].attrLength), reccol.pData + 46, sizeof(int));
				memcpy(&(column[i].attrOffset), reccol.pData + 50, sizeof(int));
				rc = GetNextRec(&rm_fileScan, &reccol);
				if (rc != SUCCESS)
					break;
			}
			break;
		}
	}
	CloseScan(&rm_fileScan);
	RM_CloseFile(&rm_column);
	//判断数据类型与表的是否一致
	for (int i = 0; i < nValues; i++) {
		if (values[nValues - 1 - i].type != column[i].attrType)
			return FIELD_TYPE_MISMATCH;
	}

	//把记录插入到数据表当中
	RM_FileHandle rm_data;
	rm_data.bOpen = false;
	rc = RM_OpenFile(relName, &rm_data);
	if (rc != SUCCESS) {
		return rc;
	}
	char * value;
	auto ctmp = column;
	RID rid;
	value = new char[rm_data.fileSubHeader->recordSize];
	values = values + nValues - 1;
	for (int i = 0; i < nValues; i++, values--, ctmp++) {//将元组暂存到value中
		memcpy(value + ctmp->attrOffset, values->data, ctmp->attrLength);
	}
	rid.bValid = false;
	InsertRec(&rm_data, value, &rid);
	delete[] value;
	delete[] column;
	RM_CloseFile(&rm_data);
	return SUCCESS;
}

RC Delete(char *relName, int nConditions, Condition *conditions) {
	RM_FileHandle rm_data, rm_table, rm_column;
	RC rc;
	RM_FileScan FileScan;
	RM_Record recdata, rectab, reccol;
	SysColumn *column, *ctmp, *ctmpleft, *ctmpright;
	Condition *contmp;
	int i, torf;//是否满足删除条件的标志
	int attrcount;//记录属性数量
	int intleft, intright;//int型的左右属性的值
	char *charleft, *charright;//char型左右属性的值
	float floatleft, floatright;//float型的左右属性的值
	AttrType attrtype;

	//判断表是否存在
	if (_access(relName, 0) == -1)
		return TABLE_NOT_EXIST;

	//打开数据表,系统表，系统列文件
	rm_data.bOpen = false;
	rc = RM_OpenFile(relName, &rm_data);
	if (rc != SUCCESS)
		return rc;
	rm_table.bOpen = false;
	rc = RM_OpenFile("SYSTABLES", &rm_table);
	if (rc != SUCCESS)
		return rc;
	rm_column.bOpen = false;
	rc = RM_OpenFile("SYSCOLUMNS", &rm_column);
	if (rc != SUCCESS)
		return rc;

	//得到系统表信息
	FileScan.bOpen = false;
	rc = OpenScan(&FileScan, &rm_table, 0, NULL);
	if (rc != SUCCESS)
		return rc;
	//遍历得到表的属性个数
	while (GetNextRec(&FileScan, &rectab) == SUCCESS) {
		if (strcmp(relName, rectab.pData) == 0) {
			memcpy(&attrcount, rectab.pData + 21, sizeof(int));
			delete[] rectab.pData;
			break;
		}
		delete[] rectab.pData;
	}

	//得到系统列信息
	FileScan.bOpen = false;
	rc = OpenScan(&FileScan, &rm_column, 0, NULL);
	if (rc != SUCCESS)
		return rc;
	column = new SysColumn[attrcount]();
	ctmp = column;
	set<string> s;
	while (GetNextRec(&FileScan, &reccol) == SUCCESS) {
		if (strcmp(relName, reccol.pData) == 0) {//读取attrcount个元组记录
			for (int i = 0; i < attrcount; i++) {
				memcpy(ctmp->tabName, reccol.pData, 21);
				memcpy(ctmp->attrName, reccol.pData + 21, 21);
				s.insert(ctmp->attrName);
				memcpy(&(ctmp->attrType), reccol.pData + 42, sizeof(AttrType));
				memcpy(&(ctmp->attrLength), reccol.pData + 42 + sizeof(AttrType), sizeof(int));
				memcpy(&(ctmp->attrOffset), reccol.pData + 42 + sizeof(int) + sizeof(AttrType), sizeof(int));
				ctmp++;
				delete[] reccol.pData;
				rc = GetNextRec(&FileScan, &reccol);
				if (rc != SUCCESS)
					break;
			}
			break;
		}
		delete[] reccol.pData;
	}
	//判断列是否存在
	for (int i = 0; i < nConditions; i++) {
		if (conditions[i].bLhsIsAttr) {
			if (s.find(conditions[i].lhsAttr.attrName) == s.end())
				return TABLE_COLUMN_ERROR;
		}
		if (conditions[i].bRhsIsAttr) {
			if (s.find(conditions[i].rhsAttr.attrName) == s.end())
				return TABLE_COLUMN_ERROR;
		}
	}

	//得到系统表信息
	FileScan.bOpen = false;
	rc = OpenScan(&FileScan, &rm_data, 0, NULL);
	if (rc != SUCCESS)
		return rc;
	while (GetNextRec(&FileScan, &recdata) == SUCCESS) {
		for (i = 0, torf = 1, contmp = conditions; i < nConditions; i++, contmp++) {//根据conditions条件进行判断
			ctmpleft = ctmpright = column;
			//左属性右值
			if (contmp->bLhsIsAttr == 1 && contmp->bRhsIsAttr == 0) {
				//遍历系统列文件，根据conditions找到各个条件对应的属性
				for (int j = 0; j < attrcount; j++) {
					if (contmp->lhsAttr.relName == NULL) {//如果未指定表名时，则缺省条件为relName
						contmp->lhsAttr.relName = new char[21];
						strcpy_s(contmp->lhsAttr.relName, 21, relName);
					}
					if ((strcmp(ctmpleft->tabName, contmp->lhsAttr.relName) == 0)
						&& (strcmp(ctmpleft->attrName, contmp->lhsAttr.attrName) == 0)) {//根据表名属性名找到对应属性
						break;
					}
					ctmpleft++;
				}
				switch (ctmpleft->attrType) {//判断属性类型并将对应值拷贝
				case ints:
					attrtype = ints;
					memcpy(&intleft, recdata.pData + ctmpleft->attrOffset, sizeof(int));
					memcpy(&intright, contmp->rhsValue.data, sizeof(int));
					break;
				case chars:
					attrtype = chars;
					charleft = new char[ctmpleft->attrLength];
					memcpy(charleft, recdata.pData + ctmpleft->attrOffset, ctmpleft->attrLength);
					charright = new char[ctmpleft->attrLength];
					memcpy(charright, contmp->rhsValue.data, ctmpleft->attrLength);
					break;
				case floats:
					attrtype = floats;
					memcpy(&floatleft, recdata.pData + ctmpleft->attrOffset, sizeof(float));
					memcpy(&floatright, contmp->rhsValue.data, sizeof(float));
					break;
				}
			}
			//右属性左值
			if (contmp->bLhsIsAttr == 0 && contmp->bRhsIsAttr == 1) {
				for (int j = 0; j < attrcount; j++) {
					if (contmp->rhsAttr.relName == NULL) {//如果未指定表名时，则缺省条件为relName
						contmp->rhsAttr.relName = new char[21];
						strcpy_s(contmp->rhsAttr.relName, 21, relName);
					}
					if ((strcmp(ctmpright->tabName, contmp->rhsAttr.relName) == 0)
						&& (strcmp(ctmpright->attrName, contmp->rhsAttr.attrName) == 0)) {//找到对应属性
						break;
					}
					ctmpright++;
				}
				switch (ctmpright->attrType) {//判断属性类型并将对应值拷贝
				case ints:
					attrtype = ints;
					memcpy(&intleft, contmp->lhsValue.data, sizeof(int));
					memcpy(&intright, recdata.pData + ctmpright->attrOffset, sizeof(int));
					break;
				case chars:
					attrtype = chars;
					charleft = new char[ctmpright->attrLength];
					memcpy(charleft, contmp->lhsValue.data, ctmpright->attrLength);
					charright = new char[ctmpright->attrLength];
					memcpy(charright, recdata.pData + ctmpright->attrOffset, ctmpright->attrLength);
					break;
				case floats:
					attrtype = floats;
					memcpy(&floatleft, contmp->lhsValue.data, sizeof(float));
					memcpy(&floatright, recdata.pData + ctmpright->attrOffset, sizeof(float));
					break;
				}
			}
			//左右均属性
			else  if (contmp->bLhsIsAttr == 1 && contmp->bRhsIsAttr == 1) {
				for (int j = 0; j < attrcount; j++) {
					if (contmp->lhsAttr.relName == NULL) {//如果未指定表名时，则缺省条件为relName
						contmp->lhsAttr.relName = new char[21];
						strcpy_s(contmp->lhsAttr.relName, 21, relName);
					}
					if ((strcmp(ctmpleft->tabName, contmp->lhsAttr.relName) == 0)
						&& (strcmp(ctmpleft->attrName, contmp->lhsAttr.attrName) == 0)) {//找到对应属性
						break;
					}
					ctmpleft++;
				}
				for (int j = 0; j < attrcount; j++) {
					if (contmp->rhsAttr.relName == NULL) {//如果未指定表名时，则缺省条件为relName
						contmp->rhsAttr.relName = new char[21];
						strcpy_s(contmp->rhsAttr.relName, 21, relName);
					}
					if ((strcmp(ctmpright->tabName, contmp->rhsAttr.relName) == 0)
						&& (strcmp(ctmpright->attrName, contmp->rhsAttr.attrName) == 0)) {//找到对应属性
						break;
					}
					ctmpright++;
				}
				switch (ctmpright->attrType) {//判断属性类型并将对应值拷贝
				case ints:
					attrtype = ints;
					memcpy(&intleft, recdata.pData + ctmpleft->attrOffset, sizeof(int));
					memcpy(&intright, recdata.pData + ctmpright->attrOffset, sizeof(int));
					break;
				case chars:
					attrtype = chars;
					charleft = new char[ctmpright->attrLength];
					memcpy(charleft, recdata.pData + ctmpleft->attrOffset, ctmpright->attrLength);
					charright = new char[ctmpright->attrLength];
					memcpy(charright, recdata.pData + ctmpright->attrOffset, ctmpright->attrLength);
					break;
				case floats:
					attrtype = floats;
					memcpy(&floatleft, recdata.pData + ctmpleft->attrOffset, sizeof(float));
					memcpy(&floatright, recdata.pData + ctmpright->attrOffset, sizeof(float));
					break;
				}
			}
			if (attrtype == ints) {
				if ((intleft == intright && contmp->op == EQual) ||
					(intleft > intright && contmp->op == GreatT) ||
					(intleft >= intright && contmp->op == GEqual) ||
					(intleft < intright && contmp->op == LessT) ||
					(intleft <= intright && contmp->op == LEqual) ||
					(intleft != intright && contmp->op == NEqual))
					torf &= 1;
				else
					torf &= 0;
			}
			else if (attrtype == chars) {
				if ((strcmp(charleft, charright) == 0 && contmp->op == EQual) ||
					(strcmp(charleft, charright) > 0 && contmp->op == GreatT) ||
					((strcmp(charleft, charright) > 0 || strcmp(charleft, charright) == 0) && contmp->op == GEqual) ||
					(strcmp(charleft, charright) < 0 && contmp->op == LessT) ||
					((strcmp(charleft, charright) < 0 || strcmp(charleft, charright) == 0) && contmp->op == LEqual) ||
					(strcmp(charleft, charright) != 0 && contmp->op == NEqual))
					torf &= 1;
				else
					torf &= 0;
				delete[] charleft;
				delete[] charright;
			}
			else if (attrtype == floats) {
				if ((floatleft == floatright && contmp->op == EQual) ||
					(floatleft > floatright && contmp->op == GreatT) ||
					(floatleft >= floatright && contmp->op == GEqual) ||
					(floatleft < floatright && contmp->op == LessT) ||
					(floatleft <= floatright && contmp->op == LEqual) ||
					(floatleft != floatright && contmp->op == NEqual))
					torf &= 1;
				else
					torf &= 0;
			}
			else
				torf &= 0;
		}

		if (torf == 1) {
			DeleteRec(&rm_data, &(recdata.rid));//删除记录
		}
		delete[] recdata.pData;
	}
	delete[] column;

	//关闭文件句柄
	rc = RM_CloseFile(&rm_table);
	if (rc != SUCCESS)
		return rc;
	rc = RM_CloseFile(&rm_column);
	if (rc != SUCCESS)
		return rc;
	rc = RM_CloseFile(&rm_data);
	if (rc != SUCCESS)
		return rc;
	return SUCCESS;
}

RC Update(char *relName, char *attrName, Value *Value, int nConditions, Condition *conditions) {
	RM_FileHandle rm_data, rm_table, rm_column;
	RC rc;
	RM_FileScan FileScan;
	RM_Record recdata, rectab, reccol;
	SysColumn *column, *ctmp, *cupdate, *ctmpleft, *ctmpright;
	Condition *contmp;
	int i, torf;//是否满足删除条件的标志
	int attrcount;//记录属性数量
	int intleft, intright;//int型的左右属性的值
	char *charleft, *charright;//char型的左右属性的值
	float floatleft, floatright;//float型的左右属性的值
	AttrType attrtype;

	//如果表不存在
	if (_access(relName, 0) == -1)
		return TABLE_NOT_EXIST;
	//打开数据表,系统表，系统列文件
	rm_data.bOpen = false;
	rc = RM_OpenFile(relName, &rm_data);
	if (rc != SUCCESS)
		return rc;
	rm_table.bOpen = false;
	rc = RM_OpenFile("SYSTABLES", &rm_table);
	if (rc != SUCCESS)
		return rc;
	rm_column.bOpen = false;
	rc = RM_OpenFile("SYSCOLUMNS", &rm_column);
	if (rc != SUCCESS)
		return rc;

	//获取系统表信息
	FileScan.bOpen = false;
	rc = OpenScan(&FileScan, &rm_table, 0, NULL);
	if (rc != SUCCESS)
		return rc;
	//查找表名为relName对应的系统表中的记录
	while (GetNextRec(&FileScan, &rectab) == SUCCESS) {
		if (strcmp(relName, rectab.pData) == 0) {
			memcpy(&attrcount, rectab.pData + 21, sizeof(int));
			delete[] rectab.pData;
			break;
		}
		delete[] rectab.pData;
	}

	//获取系统列信息
	FileScan.bOpen = false;
	rc = OpenScan(&FileScan, &rm_column, 0, NULL);
	if (rc != SUCCESS)
		return rc;
	column = new SysColumn[attrcount]();
	ctmp = column;
	set<string> s;
	while (GetNextRec(&FileScan, &reccol) == SUCCESS) {//扫描记录
		if (strcmp(relName, reccol.pData) == 0) {//读取attrcount个记录
			for (int i = 0; i < attrcount; i++) {
				memcpy(ctmp->tabName, reccol.pData, 21);
				memcpy(ctmp->attrName, reccol.pData + 21, 21);
				s.insert(ctmp->attrName);
				memcpy(&(ctmp->attrType), reccol.pData + 42, sizeof(AttrType));
				memcpy(&(ctmp->attrLength), reccol.pData + 42 + sizeof(AttrType), sizeof(int));
				memcpy(&(ctmp->attrOffset), reccol.pData + 42 + sizeof(int) + sizeof(AttrType), sizeof(int));
				if ((strcmp(relName, ctmp->tabName) == 0) && (strcmp(attrName, ctmp->attrName) == 0)) {
					cupdate = ctmp;//找到要更新的数据 对应的属性
				}
				delete[] reccol.pData;
				rc = GetNextRec(&FileScan, &reccol);
				if (rc != SUCCESS)
					break;
				ctmp++;
			}
			break;
		}
		delete[] reccol.pData;
	}
	//列不存在
	if (s.find(attrName) == s.end())
		return TABLE_COLUMN_ERROR;

	//获取系统表信息
	FileScan.bOpen = false;
	rc = OpenScan(&FileScan, &rm_data, 0, NULL);
	if (rc != SUCCESS)
		return rc;
	//查找relName对应的数据表中的记录
	while (GetNextRec(&FileScan, &recdata) == SUCCESS) {
		//遍历系统列文件，根据conditions找到各个条件对应的属性
		for (i = 0, torf = 1, contmp = conditions; i < nConditions; i++, contmp++) {
			ctmpleft = ctmpright = column;
			//左属性右值
			if (contmp->bLhsIsAttr == 1 && contmp->bRhsIsAttr == 0) {
				for (int j = 0; j < attrcount; j++) {
					if (contmp->lhsAttr.relName == NULL) {//如果未指定表名时，则缺省条件为relName
						contmp->lhsAttr.relName = new char[21];
						strcpy_s(contmp->lhsAttr.relName, 21, relName);
					}
					if ((strcmp(ctmpleft->tabName, contmp->lhsAttr.relName) == 0)
						&& (strcmp(ctmpleft->attrName, contmp->lhsAttr.attrName) == 0)) {//如果表名属性名对上，则找到对应属性
						break;
					}
					ctmpleft++;
				}
				//判断属性的类型
				if (ctmpleft->attrType == ints) {//如果属性类型是ints
					attrtype = ints;
					memcpy(&intleft, recdata.pData + ctmpleft->attrOffset, sizeof(int));
					memcpy(&intright, contmp->rhsValue.data, sizeof(int));
				}
				else if (ctmpleft->attrType == chars) {//如果属性类型是chars
					attrtype = chars;
					charleft = new char[ctmpleft->attrLength];
					memcpy(charleft, recdata.pData + ctmpleft->attrOffset, ctmpleft->attrLength);
					charright = new char[ctmpleft->attrLength];
					memcpy(charright, contmp->rhsValue.data, ctmpleft->attrLength);
				}
				else if (ctmpleft->attrType == floats) {//如果属性类是floats
					attrtype = floats;
					memcpy(&floatleft, recdata.pData + ctmpleft->attrOffset, sizeof(float));
					memcpy(&floatright, contmp->rhsValue.data, sizeof(float));
				}
				else
					torf &= 0;
			}
			//右属性左值
			else  if (contmp->bLhsIsAttr == 0 && contmp->bRhsIsAttr == 1) {
				for (int j = 0; j < attrcount; j++) {
					if (contmp->rhsAttr.relName == NULL) {//如果未指定表名时，则缺省条件为relName
						contmp->rhsAttr.relName = new char[21];
						strcpy_s(contmp->rhsAttr.relName, 21, relName);
					}
					if ((strcmp(ctmpright->tabName, contmp->rhsAttr.relName) == 0)
						&& (strcmp(ctmpright->attrName, contmp->rhsAttr.attrName) == 0)) {//如果表名属性名对上，则找到对应属性
						break;
					}
					ctmpright++;
				}
				if (ctmpright->attrType == ints) {//如果属性类型是ints
					attrtype = ints;
					memcpy(&intleft, contmp->lhsValue.data, sizeof(int));
					memcpy(&intright, recdata.pData + ctmpright->attrOffset, sizeof(int));
				}
				else if (ctmpright->attrType == chars) {//如果属性类型是chars
					attrtype = chars;
					charleft = new char[ctmpright->attrLength];
					memcpy(charleft, contmp->lhsValue.data, ctmpright->attrLength);
					charright = new char[ctmpright->attrLength];
					memcpy(charright, recdata.pData + ctmpright->attrOffset, ctmpright->attrLength);
				}
				else if (ctmpright->attrType == floats) {//如果属性类型floats
					attrtype = floats;
					memcpy(&floatleft, contmp->lhsValue.data, sizeof(float));
					memcpy(&floatright, recdata.pData + ctmpright->attrOffset, sizeof(float));
				}
				else
					torf &= 0;
			}
			//左属性右属性
			else  if (contmp->bLhsIsAttr == 1 && contmp->bRhsIsAttr == 1) {
				for (int j = 0; j < attrcount; j++) {
					if (contmp->lhsAttr.relName == NULL) {//如果未指定表名时，则缺省条件为relName
						contmp->lhsAttr.relName = new char[21];
						strcpy_s(contmp->lhsAttr.relName, 21, relName);
					}
					if ((strcmp(ctmpleft->tabName, contmp->lhsAttr.relName) == 0)
						&& (strcmp(ctmpleft->attrName, contmp->lhsAttr.attrName) == 0)) {//如果表名属性名对上，则找到对应属性
						break;
					}
					ctmpleft++;
				}
				for (int j = 0; j < attrcount; j++) {
					if (contmp->rhsAttr.relName == NULL) {//如果未指定表名时，则缺省条件为relName
						contmp->rhsAttr.relName = new char[21];
						strcpy_s(contmp->rhsAttr.relName, 21, relName);
					}
					if ((strcmp(ctmpright->tabName, contmp->rhsAttr.relName) == 0)
						&& (strcmp(ctmpright->attrName, contmp->rhsAttr.attrName) == 0)) {//如果表名属性名对上，则找到对应属性
						break;
					}
					ctmpright++;
				}
				if (ctmpright->attrType == ints && ctmpleft->attrType == ints) {//如果属性类型是ints
					attrtype = ints;
					memcpy(&intleft, recdata.pData + ctmpleft->attrOffset, sizeof(int));
					memcpy(&intright, recdata.pData + ctmpright->attrOffset, sizeof(int));
				}
				else if (ctmpright->attrType == chars && ctmpleft->attrType == chars) {//如果属性类型是chars
					attrtype = chars;
					charleft = new char[ctmpright->attrLength];
					memcpy(charleft, recdata.pData + ctmpleft->attrOffset, ctmpright->attrLength);
					charright = new char[ctmpright->attrLength];
					memcpy(charright, recdata.pData + ctmpright->attrOffset, ctmpright->attrLength);
				}
				else if (ctmpright->attrType == floats && ctmpleft->attrType == floats) {//如果属性类型是floats
					attrtype = floats;
					memcpy(&floatleft, recdata.pData + ctmpleft->attrOffset, sizeof(float));
					memcpy(&floatright, recdata.pData + ctmpright->attrOffset, sizeof(float));
				}
				else
					torf &= 0;
			}
			if (attrtype == ints) {
				if ((intleft == intright && contmp->op == EQual) ||
					(intleft > intright && contmp->op == GreatT) ||
					(intleft >= intright && contmp->op == GEqual) ||
					(intleft < intright && contmp->op == LessT) ||
					(intleft <= intright && contmp->op == LEqual) ||
					(intleft != intright && contmp->op == NEqual))
					torf &= 1;
				else
					torf &= 0;
			}
			else if (attrtype == chars) {
				if ((strcmp(charleft, charright) == 0 && contmp->op == EQual) ||
					(strcmp(charleft, charright) > 0 && contmp->op == GreatT) ||
					((strcmp(charleft, charright) > 0 || strcmp(charleft, charright) == 0) && contmp->op == GEqual) ||
					(strcmp(charleft, charright) < 0 && contmp->op == LessT) ||
					((strcmp(charleft, charright) < 0 || strcmp(charleft, charright) == 0) && contmp->op == LEqual) ||
					(strcmp(charleft, charright) != 0 && contmp->op == NEqual))
					torf &= 1;
				else
					torf &= 0;
				delete[] charleft;
				delete[] charright;
			}
			else if (attrtype == floats) {
				if ((floatleft == floatright && contmp->op == EQual) ||
					(floatleft > floatright && contmp->op == GreatT) ||
					(floatleft >= floatright && contmp->op == GEqual) ||
					(floatleft < floatright && contmp->op == LessT) ||
					(floatleft <= floatright && contmp->op == LEqual) ||
					(floatleft != floatright && contmp->op == NEqual))
					torf &= 1;
				else
					torf &= 0;
			}
			else
				torf &= 0;
		}
		if (torf == 1) {//符合条件,进行更新操作
			memcpy(recdata.pData + cupdate->attrOffset, Value->data, cupdate->attrLength);
			UpdateRec(&rm_data, &recdata);
		}
		delete[] recdata.pData;
	}

	delete[] column;

	//关闭文件句柄
	rc = RM_CloseFile(&rm_table);
	if (rc != SUCCESS)
		return rc;
	rc = RM_CloseFile(&rm_column);
	if (rc != SUCCESS)
		return rc;
	rc = RM_CloseFile(&rm_data);
	if (rc != SUCCESS)
		return rc;
	return SUCCESS;
}

bool CanButtonClick() {//需要重新实现
	return isOpened;
}

void toLowerString(std::string & s)
{
	for (unsigned i = 0; i < s.length(); i++)
	{
		s[i] = tolower(s[i]);
	}
}

void showMsg(CEditArea * editArea, char * msg)
{
	int row_num = 1;
	char **messages = new char*[row_num];
	messages[0] = msg;
	editArea->ShowMessage(row_num, messages);
	delete[] messages;
}

void showSelectResult(SelResult & res, CEditArea * editArea)
{
	/*输出执行结果*/
	int col_num = res.col_num;
	int row_num = res.row_num;
	char ** fields = new char*[col_num];
	for (int i = 0; i < col_num; i++) {
		fields[i] = new char[20];
		memcpy(fields[i], res.fields[i], 20);
	}
	char *** rows = new char**[row_num];
	for (int i = 0; i < row_num; i++) {
		rows[i] = new char*[col_num];
		for (int j = 0; j < col_num; j++) {
			AttrType type = res.type[j];
			int intNum;
			float floatNum;
			int y;
			switch (type)
			{
			case AttrType::ints:
				memcpy(&intNum, res.res[i][0] + res.offset[j], sizeof(int));
				rows[i][j] = new char[32];
				sprintf_s(rows[i][j], 32, "%d", intNum);
				break;
			case AttrType::floats:
				memcpy(&floatNum, res.res[i][0] + res.offset[j], sizeof(float));
				y = res.length[j];
				rows[i][j] = new char[32];
				sprintf_s(rows[i][j], 32, "%f", floatNum);
				break;
			case AttrType::chars:
				rows[i][j] = new char[res.length[j]];
				memcpy(rows[i][j], res.res[i][0] + res.offset[j], res.length[j]);
				break;
			default:
				break;
			}

		}
	}
	editArea->ShowSelResult(col_num, row_num, fields, rows);

	/*释放动态分配的空间*/
	for (int i = 0; i < col_num; i++) {
		delete[] fields[i];
	}
	delete[] fields;
	for (int i = 0; i < row_num; i++) {
		for (int j = 0; j < col_num; j++) {
			delete[] rows[i][j];
		}
		delete[] rows[i];
	}
	delete[] rows;
	Destory_Result(&res);
}
