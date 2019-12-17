#include "stdafx.h"
#include "EditArea.h"
#include "SYS_Manager.h"
#include "QU_Manager.h"
#include "HustBase.h"
#include "HustBaseDoc.h"
#include <iostream>

void ExecuteAndMessage(char * sql, CEditArea* editArea) {//根据执行的语句类型在界面上显示执行结果。此函数需修改
	std::string s_sql = sql;
	//查询SQL语句的处理
	if (s_sql.find("select") == 0) {
		SelResult res;
		Init_Result(&res);
		Query(sql,&res);
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

	//其他SQL语句的处理
	RC rc = execute(sql);

	//更新界面
	CHustBaseDoc *pDoc;
	pDoc = CHustBaseDoc::GetDoc();
	CHustBaseApp::pathvalue = true;
	pDoc->m_pTreeView->PopulateTree();

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
	case PF_INVALIDNAME:
		row_num = 1;
		messages = new char*[row_num];
		messages[0] = "无效文件名";
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
	else {
		AfxMessageBox(sql_str->sstr.errors);//弹出警告框，sql语句词法解析错误信息
		return rc;
	}
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
			sprintf_s(subDir, "%s", finder.GetFilePath());
			DropDB(subDir); //递归删除文件夹
			RemoveDirectory(finder.GetFilePath());
		}
		else
		{//处理文件
			DeleteFile(finder.GetFilePath());
		}
	}
	ret = RemoveDirectory(dbname);
	return ret ? SUCCESS : FAIL;
}

RC OpenDB(char *dbname) {
	SetCurrentDirectory(dbname);
	int i, j;
	if ((i = access("SYSTABLES", 0)) != 0 || (j = access("SYSCOLUMNS", 0)) != 0) {
		return FAIL;
	}
	CHustBaseApp::pathvalue = true;
	CHustBaseDoc *pDoc;
	pDoc = CHustBaseDoc::GetDoc();
	pDoc->m_pTreeView->PopulateTree();
	return SUCCESS;
}

RC CloseDB() {

	return SUCCESS;
}

/*
建表操作：
1.初始化系统表文件和系统列文件
2.创建数据表文件
*/
RC CreateTable(char *relName, int attrCount, AttrInfo *attributes) {
	RC rc;
	char  *pData;
	RM_FileHandle *rm_table, *rm_column;
	RID *rid;
	int recordsize;//数据表的每条记录的大小
	AttrInfo * attrtmp = attributes;

	/*打开系统表文件和系统列文件*/
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

	/*更新系统表文件*/
	pData = (char *)malloc(sizeof(SysTable));
	memcpy(pData, relName, 21);
	memcpy(pData + 21, &attrCount, sizeof(int));
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
	//释放申请的内存空间
	free(rm_table);
	free(pData);
	free(rid);

	/*更新系统列文件，一个表中包含多个属性列*/
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

/*
删表操作
1.把表名为relName的表删除
2.将数据库中该表对应的信息置空
*/
RC DropTable(char *relName) {
	CFile tmp;
	RM_FileHandle *rm_table, *rm_column;
	RC rc;
	RM_FileScan FileScan;
	RM_Record rectab, reccol;
	int attrcount;//临时 属性数量，属性长度，属性偏移

	/*删除数据表文件*/
	tmp.Remove((LPCTSTR)relName);

	/*将系统表和系统列中对应表的相关记录删除*/
	//打开表文件和列文件
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

RC CreateIndex(char *indexName, char *relName, char *attrName) {
	return SUCCESS;
}

RC DropIndex(char *indexName) {
	return SUCCESS;
}

RC Insert(char *relName, int nValues, Value *values) {
	RM_FileHandle *rm_data, *rm_table, *rm_column;
	char *value;//读取数据表信息
	RID *rid;
	RC rc;
	SysColumn *column, *ctmp;
	RM_FileScan FileScan;
	RM_Record rectab, reccol;
	int attrcount;//临时 属性数量，属性长度，属性偏移

	//打开数据表,系统表，系统列文件
	rm_data = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));
	rm_data->bOpen = false;
	rc = RM_OpenFile(relName, rm_data);
	if (rc != SUCCESS) {
		AfxMessageBox("打开数据表文件失败");
		return rc;
	}
	rm_table = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));
	rm_table->bOpen = false;
	rc = RM_OpenFile("SYSTABLES", rm_table);
	if (rc != SUCCESS) {
		AfxMessageBox("打开系统表文件失败");
		return rc;
	}
	rm_column = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));
	rm_column->bOpen = false;
	rc = RM_OpenFile("SYSCOLUMNS", rm_column);
	if (rc != SUCCESS) {
		AfxMessageBox("打开系统列文件失败");
		return rc;
	}
	//通过getdata函数获取系统表信息,得到的信息保存在rectab中
	FileScan.bOpen = false;
	rc = OpenScan(&FileScan, rm_table, 0, NULL);
	if (rc != SUCCESS) {
		AfxMessageBox("初始化文件扫描失败");
		return rc;
	}
	//循环查找表名为relName对应的系统表中的记录,并将记录信息保存于rectab中
	while (GetNextRec(&FileScan, &rectab) == SUCCESS) {
		if (strcmp(relName, rectab.pData) == 0) {
			memcpy(&attrcount, rectab.pData + 21, sizeof(int));
			break;
		}
	}

	//判定是否为完全插入，如果不是，返回fail
	if (attrcount != nValues) {
		AfxMessageBox("不是全纪录插入！");
		return FAIL;
	}
	//通过getdata函数获取系统列信息,得到的信息保存在reccol中
	FileScan.bOpen = false;
	rc = OpenScan(&FileScan, rm_column, 0, NULL);
	if (rc != SUCCESS) {
		AfxMessageBox("初始化数据列文件扫描失败");
		return rc;
	}
	//循环查找表名为relName对应的系统表中的记录,并将记录信息保存于rectab中
	//根据之前读取的系统表中信息，读取属性信息，结果保存在ctmp中
	column = (SysColumn *)malloc(attrcount * sizeof(SysColumn));
	ctmp = column;
	while (GetNextRec(&FileScan, &reccol) == SUCCESS) {
		if (strcmp(relName, reccol.pData) == 0) {//找到表名为relName的第一个记录，依次读取attrcount个记录
			for (int i = 0; i < attrcount; i++, ctmp++) {
				memcpy(ctmp->tabName, reccol.pData, 21);
				memcpy(ctmp->attrName, reccol.pData + 21, 21);
				memcpy(&(ctmp->attrType), reccol.pData + 42, sizeof(AttrType));
				memcpy(&(ctmp->attrLength), reccol.pData + 42 + sizeof(AttrType), sizeof(int));
				memcpy(&(ctmp->attrOffset), reccol.pData + 42 + sizeof(int) + sizeof(AttrType), sizeof(int));
				rc = GetNextRec(&FileScan, &reccol);
				if (rc != SUCCESS)
					break;
			}
			break;
		}
	}
	ctmp = column;

	//向数据表中循环插入记录
	value = (char *)malloc(rm_data->fileSubHeader->recordSize);
	values = values + nValues - 1;
	for (int i = 0; i < nValues; i++, values--, ctmp++) {
		memcpy(value + ctmp->attrOffset, values->data, ctmp->attrLength);
	}
	rid = (RID*)malloc(sizeof(RID));
	rid->bValid = false;
	InsertRec(rm_data, value, rid);
	free(value);
	free(rid);
	free(column);

	//关闭文件句柄
	rc = RM_CloseFile(rm_data);
	if (rc != SUCCESS) {
		AfxMessageBox("关闭数据表失败");
		return rc;
	}
	free(rm_data);
	rc = RM_CloseFile(rm_table);
	if (rc != SUCCESS) {
		AfxMessageBox("关闭系统表失败");
		return rc;
	}
	free(rm_table);
	rc = RM_CloseFile(rm_column);
	if (rc != SUCCESS) {
		AfxMessageBox("关闭系统列失败");
		return rc;
	}
	free(rm_column);
	return SUCCESS;
}

RC Delete(char *relName, int nConditions, Condition *conditions) {
	RM_FileHandle *rm_data, *rm_table, *rm_column;
	RC rc;
	RM_FileScan FileScan;
	RM_Record recdata, rectab, reccol;
	SysColumn *column, *ctmp, *ctmpleft, *ctmpright;
	Condition *contmp;
	int i, torf;//是否符合删除条件
	int attrcount;//临时 属性数量
	int intleft, intright;
	char *charleft, *charright;
	float floatleft, floatright;//临时 属性的值
	AttrType attrtype;

	//打开数据表,系统表，系统列文件
	rm_data = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));
	rm_data->bOpen = false;
	rc = RM_OpenFile(relName, rm_data);
	if (rc != SUCCESS)
		return rc;
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
	column = (SysColumn *)malloc(attrcount * sizeof(SysColumn));
	ctmp = column;
	while (GetNextRec(&FileScan, &reccol) == SUCCESS) {
		if (strcmp(relName, reccol.pData) == 0) {//找到表名为relName的第一个记录，依次读取attrcount个记录
			for (int i = 0; i < attrcount; i++) {
				memcpy(ctmp->tabName, reccol.pData, 21);
				memcpy(ctmp->attrName, reccol.pData + 21, 21);
				memcpy(&(ctmp->attrType), reccol.pData + 42, sizeof(AttrType));
				memcpy(&(ctmp->attrLength), reccol.pData + 42 + sizeof(AttrType), sizeof(int));
				memcpy(&(ctmp->attrOffset), reccol.pData + 42 + sizeof(int) + sizeof(AttrType), sizeof(int));
				ctmp++;
				rc = GetNextRec(&FileScan, &reccol);
				if (rc != SUCCESS)
					break;
			}
			break;
		}
	}

	//通过getdata函数获取系统表信息,得到的信息保存在recdata中
	FileScan.bOpen = false;
	rc = OpenScan(&FileScan, rm_data, 0, NULL);
	if (rc != SUCCESS)
		return rc;
	//循环查找表名为relName对应的数据表中的记录,并将记录信息保存于recdata中
	while (GetNextRec(&FileScan, &recdata) == SUCCESS) {	//取记录做判断
		for (i = 0, torf = 1, contmp = conditions; i < nConditions; i++, contmp++) {//conditions条件逐一判断
			ctmpleft = ctmpright = column;//每次循环都要将遍历整个系统列文件，找到各个条件对应的属性
			//左属性右值
			if (contmp->bLhsIsAttr == 1 && contmp->bRhsIsAttr == 0) {
				for (int j = 0; j < attrcount; j++) {//attrcount个属性逐一判断
					if (contmp->lhsAttr.relName == NULL) {//当条件中未指定表名时，默认为relName
						contmp->lhsAttr.relName = (char *)malloc(21);
						strcpy(contmp->lhsAttr.relName, relName);
					}
					if ((strcmp(ctmpleft->tabName, contmp->lhsAttr.relName) == 0)
						&& (strcmp(ctmpleft->attrName, contmp->lhsAttr.attrName) == 0)) {//根据表名属性名找到对应属性
						break;
					}
					ctmpleft++;
				}
				//对conditions的某一个条件进行判断
				switch (ctmpleft->attrType) {//判定属性的类型
				case ints:
					attrtype = ints;
					memcpy(&intleft, recdata.pData + ctmpleft->attrOffset, sizeof(int));
					memcpy(&intright, contmp->rhsValue.data, sizeof(int));
					break;
				case chars:
					attrtype = chars;
					charleft = (char *)malloc(ctmpleft->attrLength);
					memcpy(charleft, recdata.pData + ctmpleft->attrOffset, ctmpleft->attrLength);
					charright = (char *)malloc(ctmpleft->attrLength);
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
				for (int j = 0; j < attrcount; j++) {//attrcount个属性逐一判断
					if (contmp->rhsAttr.relName == NULL) {//当条件中未指定表名时，默认为relName
						contmp->rhsAttr.relName = (char *)malloc(21);
						strcpy(contmp->rhsAttr.relName, relName);
					}
					if ((strcmp(ctmpright->tabName, contmp->rhsAttr.relName) == 0)
						&& (strcmp(ctmpright->attrName, contmp->rhsAttr.attrName) == 0)) {//根据表名属性名找到对应属性
						break;
					}
					ctmpright++;
				}
				//对conditions的某一个条件进行判断
				switch (ctmpright->attrType) {
				case ints:
					attrtype = ints;
					memcpy(&intleft, contmp->lhsValue.data, sizeof(int));
					memcpy(&intright, recdata.pData + ctmpright->attrOffset, sizeof(int));
					break;
				case chars:
					attrtype = chars;
					charleft = (char *)malloc(ctmpright->attrLength);
					memcpy(charleft, contmp->lhsValue.data, ctmpright->attrLength);
					charright = (char *)malloc(ctmpright->attrLength);
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
				for (int j = 0; j < attrcount; j++) {//attrcount个属性逐一判断
					if (contmp->lhsAttr.relName == NULL) {//当条件中未指定表名时，默认为relName
						contmp->lhsAttr.relName = (char *)malloc(21);
						strcpy(contmp->lhsAttr.relName, relName);
					}
					if ((strcmp(ctmpleft->tabName, contmp->lhsAttr.relName) == 0)
						&& (strcmp(ctmpleft->attrName, contmp->lhsAttr.attrName) == 0)) {//根据表名属性名找到对应属性
						break;
					}
					ctmpleft++;
				}
				for (int j = 0; j < attrcount; j++) {//attrcount个属性逐一判断
					if (contmp->rhsAttr.relName == NULL) {//当条件中未指定表名时，默认为relName
						contmp->rhsAttr.relName = (char *)malloc(21);
						strcpy(contmp->rhsAttr.relName, relName);
					}
					if ((strcmp(ctmpright->tabName, contmp->rhsAttr.relName) == 0)
						&& (strcmp(ctmpright->attrName, contmp->rhsAttr.attrName) == 0)) {//根据表名属性名找到对应属性
						break;
					}
					ctmpright++;
				}
				//对conditions的某一个条件进行判断
				switch (ctmpright->attrType) {
				case ints:
					attrtype = ints;
					memcpy(&intleft, recdata.pData + ctmpleft->attrOffset, sizeof(int));
					memcpy(&intright, recdata.pData + ctmpright->attrOffset, sizeof(int));
					break;
				case chars:
					attrtype = chars;
					charleft = (char *)malloc(ctmpright->attrLength);
					memcpy(charleft, recdata.pData + ctmpleft->attrOffset, ctmpright->attrLength);
					charright = (char *)malloc(ctmpright->attrLength);
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
				free(charleft);
				free(charright);
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
			DeleteRec(rm_data, &(recdata.rid));
		}
	}
	free(column);

	//关闭文件句柄
	rc = RM_CloseFile(rm_table);
	if (rc != SUCCESS)
		return rc;
	free(rm_table);
	rc = RM_CloseFile(rm_column);
	if (rc != SUCCESS)
		return rc;
	free(rm_column);
	rc = RM_CloseFile(rm_data);
	if (rc != SUCCESS)
		return rc;
	free(rm_data);
	return SUCCESS;
}

RC Update(char *relName, char *attrName, Value *Value, int nConditions, Condition *conditions) {
	//只能进行单值更新
	RM_FileHandle *rm_data, *rm_table, *rm_column;
	RC rc;
	RM_FileScan FileScan;
	RM_Record recdata, rectab, reccol;
	SysColumn *column, *ctmp, *cupdate, *ctmpleft, *ctmpright;
	Condition *contmp;
	int i, torf;//是否符合删除条件
	int attrcount;//临时 属性数量
	int intleft, intright;
	char *charleft, *charright;
	float floatleft, floatright;//临时 属性的值
	AttrType attrtype;

	//打开数据表,系统表，系统列文件
	rm_data = (RM_FileHandle *)malloc(sizeof(RM_FileHandle));
	rm_data->bOpen = false;
	rc = RM_OpenFile(relName, rm_data);
	if (rc != SUCCESS)
		return rc;
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
	column = (SysColumn *)malloc(attrcount * sizeof(SysColumn));
	cupdate = (SysColumn *)malloc(sizeof(SysColumn));
	ctmp = column;
	while (GetNextRec(&FileScan, &reccol) == SUCCESS) {
		if (strcmp(relName, reccol.pData) == 0) {//找到表名为relName的第一个记录，依次读取attrcount个记录
			for (int i = 0; i < attrcount; i++) {
				memcpy(ctmp->tabName, reccol.pData, 21);
				memcpy(ctmp->attrName, reccol.pData + 21, 21);
				memcpy(&(ctmp->attrType), reccol.pData + 42, sizeof(AttrType));
				memcpy(&(ctmp->attrLength), reccol.pData + 42 + sizeof(AttrType), sizeof(int));
				memcpy(&(ctmp->attrOffset), reccol.pData + 42 + sizeof(int) + sizeof(AttrType), sizeof(int));
				if ((strcmp(relName, ctmp->tabName) == 0) && (strcmp(attrName, ctmp->attrName) == 0)) {
					cupdate = ctmp;//找到要更新数据 对应的属性
				}
				rc = GetNextRec(&FileScan, &reccol);
				if (rc != SUCCESS)
					break;
				ctmp++;
			}
			break;
		}
	}

	//通过getdata函数获取系统表信息,得到的信息保存在recdata中
	FileScan.bOpen = false;
	rc = OpenScan(&FileScan, rm_data, 0, NULL);
	if (rc != SUCCESS)
		return rc;
	//循环查找表名为relName对应的数据表中的记录,并将记录信息保存于recdata中
	while (GetNextRec(&FileScan, &recdata) == SUCCESS) {
		for (i = 0, torf = 1, contmp = conditions; i < nConditions; i++, contmp++) {//conditions条件逐一判断
			ctmpleft = ctmpright = column;//每次循环都要将遍历整个系统列文件，找到各个条件对应的属性
			//左属性右值
			if (contmp->bLhsIsAttr == 1 && contmp->bRhsIsAttr == 0) {
				for (int j = 0; j < attrcount; j++) {//attrcount个属性逐一判断
					if (contmp->lhsAttr.relName == NULL) {//当条件中未指定表名时，默认为relName
						contmp->lhsAttr.relName = (char *)malloc(21);
						strcpy(contmp->lhsAttr.relName, relName);
					}
					if ((strcmp(ctmpleft->tabName, contmp->lhsAttr.relName) == 0)
						&& (strcmp(ctmpleft->attrName, contmp->lhsAttr.attrName) == 0)) {//根据表名属性名找到对应属性
						break;
					}
					ctmpleft++;
				}
				//对conditions的某一个条件进行判断
				if (ctmpleft->attrType == ints) {//判定属性的类型
					attrtype = ints;
					memcpy(&intleft, recdata.pData + ctmpleft->attrOffset, sizeof(int));
					memcpy(&intright, contmp->rhsValue.data, sizeof(int));
				}
				else if (ctmpleft->attrType == chars) {
					attrtype = chars;
					charleft = (char *)malloc(ctmpleft->attrLength);
					memcpy(charleft, recdata.pData + ctmpleft->attrOffset, ctmpleft->attrLength);
					charright = (char *)malloc(ctmpleft->attrLength);
					memcpy(charright, contmp->rhsValue.data, ctmpleft->attrLength);
				}
				else if (ctmpleft->attrType == floats) {
					attrtype = floats;
					memcpy(&floatleft, recdata.pData + ctmpleft->attrOffset, sizeof(float));
					memcpy(&floatright, contmp->rhsValue.data, sizeof(float));
				}
				else
					torf &= 0;
			}
			//右属性左值
			else  if (contmp->bLhsIsAttr == 0 && contmp->bRhsIsAttr == 1) {
				for (int j = 0; j < attrcount; j++) {//attrcount个属性逐一判断
					if (contmp->rhsAttr.relName == NULL) {//当条件中未指定表名时，默认为relName
						contmp->rhsAttr.relName = (char *)malloc(21);
						strcpy(contmp->rhsAttr.relName, relName);
					}
					if ((strcmp(ctmpright->tabName, contmp->rhsAttr.relName) == 0)
						&& (strcmp(ctmpright->attrName, contmp->rhsAttr.attrName) == 0)) {//根据表名属性名找到对应属性
						break;
					}
					ctmpright++;
				}
				//对conditions的某一个条件进行判断
				if (ctmpright->attrType == ints) {//判定属性的类型
					attrtype = ints;
					memcpy(&intleft, contmp->lhsValue.data, sizeof(int));
					memcpy(&intright, recdata.pData + ctmpright->attrOffset, sizeof(int));
				}
				else if (ctmpright->attrType == chars) {
					attrtype = chars;
					charleft = (char *)malloc(ctmpright->attrLength);
					memcpy(charleft, contmp->lhsValue.data, ctmpright->attrLength);
					charright = (char *)malloc(ctmpright->attrLength);
					memcpy(charright, recdata.pData + ctmpright->attrOffset, ctmpright->attrLength);
				}
				else if (ctmpright->attrType == floats) {
					attrtype = floats;
					memcpy(&floatleft, contmp->lhsValue.data, sizeof(float));
					memcpy(&floatright, recdata.pData + ctmpright->attrOffset, sizeof(float));
				}
				else
					torf &= 0;
			}
			//左右均属性
			else  if (contmp->bLhsIsAttr == 1 && contmp->bRhsIsAttr == 1) {
				for (int j = 0; j < attrcount; j++) {//attrcount个属性逐一判断
					if (contmp->lhsAttr.relName == NULL) {//当条件中未指定表名时，默认为relName
						contmp->lhsAttr.relName = (char *)malloc(21);
						strcpy(contmp->lhsAttr.relName, relName);
					}
					if ((strcmp(ctmpleft->tabName, contmp->lhsAttr.relName) == 0)
						&& (strcmp(ctmpleft->attrName, contmp->lhsAttr.attrName) == 0)) {//根据表名属性名找到对应属性
						break;
					}
					ctmpleft++;
				}
				for (int j = 0; j < attrcount; j++) {//attrcount个属性逐一判断
					if (contmp->rhsAttr.relName == NULL) {//当条件中未指定表名时，默认为relName
						contmp->rhsAttr.relName = (char *)malloc(21);
						strcpy(contmp->rhsAttr.relName, relName);
					}
					if ((strcmp(ctmpright->tabName, contmp->rhsAttr.relName) == 0)
						&& (strcmp(ctmpright->attrName, contmp->rhsAttr.attrName) == 0)) {//根据表名属性名找到对应属性
						break;
					}
					ctmpright++;
				}
				//对conditions的某一个条件进行判断
				if (ctmpright->attrType == ints && ctmpleft->attrType == ints) {//判定属性的类型
					attrtype = ints;
					memcpy(&intleft, recdata.pData + ctmpleft->attrOffset, sizeof(int));
					memcpy(&intright, recdata.pData + ctmpright->attrOffset, sizeof(int));
				}
				else if (ctmpright->attrType == chars && ctmpleft->attrType == chars) {
					attrtype = chars;
					charleft = (char *)malloc(ctmpright->attrLength);
					memcpy(charleft, recdata.pData + ctmpleft->attrOffset, ctmpright->attrLength);
					charright = (char *)malloc(ctmpright->attrLength);
					memcpy(charright, recdata.pData + ctmpright->attrOffset, ctmpright->attrLength);
				}
				else if (ctmpright->attrType == floats && ctmpleft->attrType == floats) {
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
				free(charleft);
				free(charright);
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
			memcpy(recdata.pData + cupdate->attrOffset, Value->data, cupdate->attrLength);
			UpdateRec(rm_data, &recdata);
		}
	}

	free(column);
	//关闭文件句柄
	rc = RM_CloseFile(rm_table);
	if (rc != SUCCESS)
		return rc;
	free(rm_table);
	rc = RM_CloseFile(rm_column);
	if (rc != SUCCESS)
		return rc;
	free(rm_column);
	rc = RM_CloseFile(rm_data);
	if (rc != SUCCESS)
		return rc;
	free(rm_data);
	return SUCCESS;
}

bool CanButtonClick() {//需要重新实现
	//如果当前有数据库已经打开
	return true;
	//如果当前没有数据库打开
	//return false;
}
