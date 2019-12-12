#include "stdafx.h"
#include "EditArea.h"
#include "SYS_Manager.h"
#include "QU_Manager.h"
#include <iostream>

void ExecuteAndMessage(char * sql, CEditArea* editArea) {//根据执行的语句类型在界面上显示执行结果。此函数需修改
	std::string s_sql = sql;
	if (s_sql.find("select") == 0) {
		SelResult res;
		Init_Result(&res);
		//rc = Query(sql,&res);
		//将查询结果处理一下，整理成下面这种形式
		//调用editArea->ShowSelResult(col_num,row_num,fields,rows);
		int col_num = 5;
		int row_num = 3;
		char ** fields = new char *[5];
		for (int i = 0; i < col_num; i++) {
			fields[i] = new char[20];
			memset(fields[i], 0, 20);
			fields[i][0] = 'f';
			fields[i][1] = i + '0';
		}
		char *** rows = new char**[row_num];
		for (int i = 0; i < row_num; i++) {
			rows[i] = new char*[col_num];
			for (int j = 0; j < col_num; j++) {
				rows[i][j] = new char[20];
				memset(rows[i][j], 0, 20);
				rows[i][j][0] = 'r';
				rows[i][j][1] = i + '0';
				rows[i][j][2] = '+';
				rows[i][j][3] = j + '0';
			}
		}
		editArea->ShowSelResult(col_num, row_num, fields, rows);
		for (int i = 0; i < 5; i++) {
			delete[] fields[i];
		}
		delete[] fields;
		Destory_Result(&res);
		return;
	}
	RC rc = execute(sql);
	int row_num = 0;
	char**messages;
	switch (rc) {
	case SUCCESS:
		row_num = 1;
		messages = new char*[row_num];
		messages[0] = "操作成功";
		editArea->ShowMessage(row_num, messages);
		delete[] messages;
		break;
	case SQL_SYNTAX:
		row_num = 1;
		messages = new char*[row_num];
		messages[0] = "有语法错误";
		editArea->ShowMessage(row_num, messages);
		delete[] messages;
		break;
	default:
		row_num = 1;
		messages = new char*[row_num];
		messages[0] = "功能未实现";
		editArea->ShowMessage(row_num, messages);
		delete[] messages;
		break;
	}
}

RC execute(char * sql) {
	sqlstr *sql_str = NULL;
	RC rc;
	sql_str = get_sqlstr();
	rc = parse(sql, sql_str);//只有两种返回结果SUCCESS和SQL_SYNTAX

	if (rc == SUCCESS)
	{
		int i = 0;
		switch (sql_str->flag)
		{
			//case 1:
			////判断SQL语句为select语句

			//break;

		case 2:
			//判断SQL语句为insert语句

		case 3:
			//判断SQL语句为update语句
			break;

		case 4:
			//判断SQL语句为delete语句
			break;

		case 5:
			//判断SQL语句为createTable语句
			CreateTable(sql_str->sstr.cret.relName, sql_str->sstr.cret.attrCount, sql_str->sstr.cret.attributes);
			break;

		case 6:
			//判断SQL语句为dropTable语句
			DropTable(sql_str->sstr.drt.relName);
			break;

		case 7:
			//判断SQL语句为createIndex语句
			break;

		case 8:
			//判断SQL语句为dropIndex语句
			break;

		case 9:
			//判断为help语句，可以给出帮助提示
			break;

		case 10:
			//判断为exit语句，可以由此进行退出操作
			break;
		}
	}
	else {
		AfxMessageBox(sql_str->sstr.errors);//弹出警告框，sql语句词法解析错误信息
		return rc;
	}
}

RC CreateDB(char *dbpath, char *dbname) {
	//设置当前目录为dbpath，其中dbPath包含有dbName；
	SetCurrentDirectory(dbpath);
	RC rc;
	//创建系统表文件和系统列文件
	rc = RM_CreateFile("SYSTABLES", sizeof(SysTable));
	if (rc != SUCCESS)
		return rc;
	rc = RM_CreateFile("SYSCOLUMNS", sizeof(SysColumn));
	if (rc != SUCCESS)
		return rc;
	return SUCCESS;
}

RC DropDB(char *dbname) {
	CFileFind tempFind;
	char sTempFileFind[200];
	sprintf_s(sTempFileFind, "%s\\*.*", dbname);
	BOOL IsFinded = tempFind.FindFile(sTempFileFind);
	while (IsFinded)
	{
		IsFinded = tempFind.FindNextFile();
		if (!tempFind.IsDots())
		{
			char sFoundFileName[200];
			strcpy_s(sFoundFileName, tempFind.GetFileName().GetBuffer(200));
			if (tempFind.IsDirectory())
			{
				char sTempDir[200];
				sprintf_s(sTempDir, "%s\\%s", dbname, sFoundFileName);
				DropTable(sTempDir);
			}
			else
			{
				char sTempFileName[200];
				sprintf_s(sTempFileName, "%s\\%s", dbname, sFoundFileName);
				DeleteFile(sTempFileName);
			}
		}
	}
	tempFind.Close();
	if (!RemoveDirectory(dbname))
	{
		return FAIL;
	}
	return SUCCESS;
}

RC OpenDB(char *dbname) {
	return SUCCESS;
}

RC CloseDB() {
	return SUCCESS;
}

/*
建表操作：
1.系统表文件和系统列文件的初始化
2.创建数据表文件
*/
//done
RC CreateTable(char *relName, int attrCount, AttrInfo *attributes) {
	RC rc;
	char  *pData;
	RM_FileHandle *rm_table, *rm_column;
	RID *rid;
	int recordsize;//数据表的每条记录的大小
	AttrInfo * attrtmp = attributes;

	//打开系统表文件和系统列文件
	rm_table = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));
	rm_table->bOpen = false;
	rc = RM_OpenFile("SYSTABLES", rm_table);
	if (rc != SUCCESS)
		return rc;
	rm_column = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));
	rm_column->bOpen = false;
	rc = RM_OpenFile("SYSCOLUMNS", rm_column);
	if (rc != SUCCESS)
		return rc;
	//向系统表中填充信息
	pData = (char *)malloc(sizeof(SysTable));
	memcpy(pData, relName, 21);//填充表名
	memcpy(pData + 21, &attrCount, sizeof(int));//填充属性列数
	rid = (RID *)malloc(sizeof(RID));
	rid->bValid = false;
	rc = InsertRec(rm_table, pData, rid);
	if (rc != SUCCESS) {
		return rc;
	}
	rc = RM_CloseFile(rm_table);
	if (rc != SUCCESS) {
		return rc;
	}
	free(rm_table);
	free(pData);
	free(rid);//释放申请的内存空间

	//向系统列中循环填充信息 一个表中包含多个属性列，就需要循环
	for (int i = 0, offset = 0; i < attrCount; i++, attrtmp++) {
		pData = (char *)malloc(sizeof(SysColumn));
		memcpy(pData, relName, 21);
		memcpy(pData + 21, attrtmp->attrName, 21);
		memcpy(pData + 42, &(attrtmp->attrType), sizeof(AttrType));
		memcpy(pData + 42 + sizeof(AttrType), &(attrtmp->attrLength), sizeof(int));
		memcpy(pData + 42 + sizeof(int) + sizeof(AttrType), &offset, sizeof(int));
		memcpy(pData + 42 + 2 * sizeof(int) + sizeof(AttrType), "0", sizeof(char));
		rid = (RID *)malloc(sizeof(RID));
		rid->bValid = false;
		rc = InsertRec(rm_column, pData, rid);
		if (rc != SUCCESS) {
			return rc;
		}
		free(pData);
		free(rid);
		offset += attrtmp->attrLength;
	}
	rc = RM_CloseFile(rm_column);
	if (rc != SUCCESS) {
		return rc;
	}
	free(rm_column);
	//创建数据表
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

//删表操作
//1.把表名为relName的表删除 
//2.将数据库中该表对应的信息置空
RC DropTable(char *relName) {
	CFile tmp;
	RM_FileHandle *rm_table, *rm_column;
	RC rc;
	RM_FileScan FileScan;
	RM_Record rectab, reccol;
	int attrcount;//临时 属性数量，属性长度，属性偏移

	//删除数据表文件
	tmp.Remove((LPCTSTR)relName);

	//将系统表和系统列中对应表的相关记录删除
	//打开系统表，系统列文件
	rm_table = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));
	rm_table->bOpen = false;
	rc = RM_OpenFile("SYSTABLES", rm_table);
	if (rc != SUCCESS)
		return rc;
	rm_column = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));
	rm_column->bOpen = false;
	rc = RM_OpenFile("SYSCOLUMNS", rm_column);
	if (rc != SUCCESS)
		return rc;

	//通过getdata函数获取系统表信息,得到的信息保存在rectab中
	FileScan.bOpen = false;
	rc = OpenScan(&FileScan, rm_table, 0, NULL);
	if (rc != SUCCESS)
		return rc;
	//循环查找表名为relName对应的系统表中的记录,并将记录信息保存于rectab中
	while (GetNextRec(&FileScan, &rectab) == SUCCESS) {
		if (strcmp(relName, rectab.pData) == 0) {
			memcpy(&attrcount, rectab.pData + 21, sizeof(int));
			DeleteRec(rm_table, &(rectab.rid));
			break;
		}
	}
	//通过getdata函数获取系统列信息,得到的信息保存在reccol中
	FileScan.bOpen = false;
	rc = OpenScan(&FileScan, rm_column, 0, NULL);
	if (rc != SUCCESS)
		return rc;
	//循环查找表名为relName对应的系统表中的记录,并将记录信息保存于rectab中
	//根据之前读取的系统表中信息，读取属性信息，结果保存在ctmp中
	for (int i = 0; i < attrcount && GetNextRec(&FileScan, &reccol) == SUCCESS;) {
		if (strcmp(relName, reccol.pData) == 0) {//找到表名为relName的第一个记录，依次读取attrcount个记录
			DeleteRec(rm_column, &(reccol.rid));
			i++;
		}
	}

	//关闭文件句柄
	rc = RM_CloseFile(rm_table);
	if (rc != SUCCESS)
		return rc;
	free(rm_table);
	rc = RM_CloseFile(rm_column);
	if (rc != SUCCESS)
		return rc;
	free(rm_column);
	return SUCCESS;
}

bool CanButtonClick() {//需要重新实现
	//如果当前有数据库已经打开
	return true;
	//如果当前没有数据库打开
	//return false;
}
