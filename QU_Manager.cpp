#include "StdAfx.h"
#include "QU_Manager.h"
#include "RM_Manager.h"

//select功能的全局静态变量
static int *_recordcount;//各个数据表包含的记录数量
static int tablecount; //表数
static int counterIndex;
static int *counter;
typedef struct printrel {
	int i;	//保存属性对应表名对应的下标
	SysColumn col;//保存属性信息
}printrel;
void handle();

void Init_Result(SelResult * res) {
	res->next_res = NULL;
}

void Destory_Result(SelResult * res) {
	for (int i = 0; i < res->row_num; i++) {
		for (int j = 0; j < res->col_num; j++) {
			delete[] res->res[i][j];
		}
		delete[] res->res[i];
	}
	if (res->next_res != NULL) {
		Destory_Result(res->next_res);
	}
}

RC Query(char * sql, SelResult * res) {
	sqlstr *sql_str = NULL;
	RC rc;
	sql_str = get_sqlstr();
	rc = parse(sql, sql_str);//只有两种返回结果SUCCESS和SQL_SYNTAX
	if (rc == SUCCESS) {
		char tmp1[20], *tmp;
		rc = Select(sql_str->sstr.sel.nSelAttrs, sql_str->sstr.sel.selAttrs, sql_str->sstr.sel.nRelations, sql_str->sstr.sel.relations, sql_str->sstr.sel.nConditions, sql_str->sstr.sel.conditions, res);
	}
	return rc;
}

RC Select(int nSelAttrs, RelAttr ** selAttrs, int nRelations, char ** relations, int nConditions, Condition * conditions, SelResult * res)
{
	RM_FileHandle *rm_data, *rm_table, *rm_column;
	RM_FileScan FileScan;
	RM_FileScan *relscan;
	RM_Record recdata, rectab, reccol;
	RM_Record *recdkr;//保存一条笛卡尔记录
	SysColumn *column, *ctmp, *ctmpleft, *ctmpright;
	Condition *contmp;
	int allattrcount = 0;//涉及的nRelations个数据表的属性数量之和
	int *attrcount;//各个数据表包含的属性数量
	int allcount = 1;//笛卡尔积的数量
	int torf;//是否符合删除条件
	int resultcount = 0;//结果数
	int intvalue, intleft, intright;
	char *charvalue, *charleft, *charright;
	float floatvalue, floatleft, floatright;
	RC rc;
	printrel *pr;
	char *res;
	AttrType attrtype;
	char *attention;

	pr = (printrel*)malloc(nSelAttrs * sizeof(printrel));
	//打开数据表,系统表，系统列文件
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
	//打开nRelations个数据表，并返回文件句柄到rm_data + i中
	rm_data = (RM_FileHandle *)malloc(nRelations * sizeof(RM_FileHandle));
	for (int i = 0; i < nRelations; i++) {
		(rm_data + i)->bOpen = false;
		rc = RM_OpenFile(relations[i], rm_data + i);
		if (rc != SUCCESS)
			return rc;
	}//注意顺序： rm_data -> relations   : 0-(nRelations-1)  -> 0-(nRelations-1)

	//读取系统表，获取属性数量信息，
	//总数量保存于_allattrcount,各个关系的属性数量保存于*（_attrcount + i ）中
	attrcount = (int *)malloc(nRelations * sizeof(int));
	//找到各个表名对应的属性数量，并保存在_attrcount数组中
	for (int i = 0; i < nRelations; i++) {
		FileScan.bOpen = false;
		rc = OpenScan(&FileScan, rm_table, 0, NULL);//每次查找初始化！
		if (rc != SUCCESS)
			return rc;
		while (GetNextRec(&FileScan, &rectab) == SUCCESS) {
			if (strcmp(relations[i], rectab.pData) == 0) {
				memcpy(attrcount + i, rectab.pData + 21, sizeof(int));
				allattrcount += *(attrcount + i);//计算所涉及关系的属性的总和，保存在_allattrcount中
				break;
			}
		}
	}//注意顺序：_attrcount -> relations : 0-(nRelations-1) -> 0-(nRelations-1)
	//读取系统列，获取涉及数据表的全部属性信息
	//保存于以column为起始地址的空间中，顺序是按照relations表名的顺序
	column = (SysColumn *)malloc(allattrcount * sizeof(SysColumn));//申请_allattrcount个Syscolumn空间，用于保存nRealtions个关系的属性
	ctmp = column;
	for (int i = 0; i < nRelations; i++) {
		FileScan.bOpen = false;
		rc = OpenScan(&FileScan, rm_column, 0, NULL);
		if (rc != SUCCESS)
			return rc;//查找每个表的属性信息都要初始化扫描
		while (GetNextRec(&FileScan, &reccol) == SUCCESS) {
			if (strcmp(relations[i], reccol.pData) == 0) {//找到表名为*（relations + i）的第一个记录
				for (int j = 0; j < *(attrcount + i); j++) {  //依次读取*（_attrcount + i）个属性
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
	}//column -> relations : 0-(nRelations-1) -> 0-(nRelations-1)
	//获取selAttrs对应的笛卡尔积的位置

	for (int i = 0; i < nSelAttrs; i++) {
		for (int j = 0; j < nRelations; j++) {
			ctmp = column;
			if (strcmp(selAttrs[i]->relName, relations[j]) == 0) {
				pr[i].i = j;
				for (int k = 0; k < allattrcount; k++, ctmp++) {
					if (strcmp(selAttrs[i]->relName, ctmp->tabName) == 0 && strcmp(selAttrs[i]->attrName, ctmp->attrName) == 0) {
						pr[i].col = *ctmp;
						break;
					}
				}
				break;
			}
		}
	}
	//读取数据表，获取记录数
	//保存在recordcount中，顺序是按照表名relations的顺序
	_recordcount = (int *)malloc(nRelations * sizeof(int));
	for (int i = 0; i < nRelations; i++) {
		FileScan.bOpen = false;
		rc = OpenScan(&FileScan, rm_data + i, 0, NULL);
		if (rc != SUCCESS)
			return rc;//查找每个表的属性信息都要初始化扫描
		*(_recordcount + i) = 0;
		while (GetNextRec(&FileScan, &recdata) == SUCCESS) {
			*(_recordcount + i) += 1;
		}
	}
	//提取的信息包括各数据表属性数量，各数据表属性信息，各数据表的记录数
	//第一个字符串保存属性数量，第二个字符串保存记录数，接下来的nSelAttrs个字节保存属性名
	res = (char *)malloc((20 * 20 + 2 + nSelAttrs) * 20);
	*RES = res;
	charvalue = (char *)malloc(20);
	itoa(nSelAttrs, charvalue, 10);
	memcpy(res, charvalue, 20);
	free(charvalue);
	for (int i = 0; i < nSelAttrs; i++) {
		memcpy(res + i * 20 + 40, selAttrs[i]->attrName, 20);
	}
	char *data = res + (2 + nSelAttrs) * 20;
	//通过笛卡尔算法来实现各个关系的连接
	counter = (int *)malloc(nRelations * sizeof(int));
	for (int i = 0; i < nRelations; i++) {
		counter[i] = 0;
	}
	counterIndex = nRelations - 1;
	tablecount = nRelations;
	relscan = (RM_FileScan *)malloc(nRelations * sizeof(RM_FileScan));
	recdkr = (RM_Record *)malloc(nRelations * sizeof(RM_Record));
	//对nRelations个数据表分别初始化扫描，并计算笛卡尔积的条目数
	for (int i = 0; i < nRelations; i++) {
		allcount *= _recordcount[i];
		(relscan + i)->bOpen = false;
		rc = OpenScan(relscan + i, rm_data + i, 0, NULL);
		if (rc != SUCCESS)
			return rc;
	}

	for (int i = 0; i < allcount; i++) {
		//取出一条笛卡尔记录，一条笛卡尔记录对应nRelations个表中的一条记录
		//取出relations +j的数据表的的第counter[j]个记录，放在recdkr + j处
		//处理一条记录之后要将relscan 重新初始化扫描
		for (int j = 0; j < nRelations; j++) {
			for (int k = 0; k != counter[j] + 1; k++) {
				GetNextRec(relscan + j, recdkr + j);
			}
			(relscan + j)->bOpen = false;
			rc = OpenScan(relscan + j, rm_data + j, 0, NULL);
			if (rc != SUCCESS)
				return rc;
		}
		//进行判断
		torf = 1;
		contmp = conditions;
		for (int x = 0; x < nConditions; x++, contmp++) {//对nSelAttrs个条件逐一判断
			ctmpleft = ctmpright = column;//将ctmp重置到column处
			//左属性右值
			if (contmp->bLhsIsAttr == 1 && contmp->bRhsIsAttr == 0) {
				for (int y = 0; y < allattrcount; y++, ctmpleft++) {//从attrcount个属性找到对应于conditions表名和属性名对应的记录
					if ((strcmp(ctmpleft->tabName, contmp->lhsAttr.relName) == 0)//找到条件中的属性对应系统表中的属性相关信息
						&& (strcmp(ctmpleft->attrName, contmp->lhsAttr.attrName) == 0)) {
						break;
					}
					if (y == allattrcount - 1) {
						attention = strcat(contmp->lhsAttr.relName, ".");
						attention = strcat(attention, contmp->lhsAttr.attrName);
						attention = strcat(attention, "\r\n不存在，请检查sql语句");
						AfxMessageBox(attention);
						return FAIL;
					}
				}

				for (int z = 0; z < nRelations; z++) {//找到这个条件的属性对应的表在relations数组中的下标位置    
					if (strcmp(relations[z], ctmpleft->tabName) == 0) {
						//对conditions的某一个条件进行判断
						if (ctmpleft->attrType == ints) {//判定属性的类型
							attrtype = ints;
							memcpy(&intleft, (recdkr + z)->pData + ctmpleft->attrOffset, sizeof(int));
							memcpy(&intright, contmp->rhsValue.data, sizeof(int));
						}
						else if (ctmpleft->attrType == chars) {
							attrtype = chars;
							charleft = (char *)malloc(ctmpleft->attrLength);
							memcpy(charleft, (recdkr + z)->pData + ctmpleft->attrOffset, ctmpleft->attrLength);
							charright = (char *)malloc(ctmpleft->attrLength);
							memcpy(charright, contmp->rhsValue.data, ctmpleft->attrLength);
						}
						else {
							attrtype = floats;
							memcpy(&floatleft, (recdkr + z)->pData + ctmpleft->attrOffset, sizeof(float));
							memcpy(&floatright, contmp->rhsValue.data, sizeof(float));
						}
						break;
					}
				}
			}

			//右属性左值
			else  if (contmp->bLhsIsAttr == 0 && contmp->bRhsIsAttr == 1) {
				for (int y = 0; y < allattrcount; y++, ctmpright++) {//attrcount个属性逐一判断
					if ((strcmp(ctmpright->tabName, contmp->rhsAttr.relName) == 0)
						&& (strcmp(ctmpright->attrName, contmp->rhsAttr.attrName) == 0)) {//根据表名属性名找到对应属性
						break;
					}
					if (y == allattrcount - 1) {
						attention = strcat(contmp->rhsAttr.relName, ".");
						attention = strcat(attention, contmp->rhsAttr.attrName);
						attention = strcat(attention, "\r\n不存在，请检查sql语句");
						AfxMessageBox(attention);
						return FAIL;
					}
				}
				for (int z = 0; z < nRelations; z++) {//找到这个条件的属性对应的表在relations数组中的下标位置    
					if (strcmp(relations[z], ctmpleft->tabName) == 0) {
						//对conditions的某一个条件进行判断
						if (ctmpright->attrType == ints) {//判定属性的类型
							attrtype = ints;
							memcpy(&intleft, contmp->lhsValue.data, sizeof(int));
							memcpy(&intright, (recdkr + z)->pData + ctmpright->attrOffset, sizeof(int));
						}
						else if (ctmpleft->attrType == chars) {
							attrtype = chars;
							charleft = (char *)malloc(ctmpleft->attrLength);
							memcpy(charleft, contmp->lhsValue.data, ctmpleft->attrLength);
							charright = (char *)malloc(ctmpleft->attrLength);
							memcpy(charright, (recdkr + z)->pData + ctmpright->attrOffset, ctmpright->attrLength);
						}
						else {
							attrtype = floats;
							memcpy(&floatleft, contmp->lhsValue.data, sizeof(float));
							memcpy(&floatright, (recdkr + z)->pData + ctmpright->attrOffset, sizeof(float));
						}
						break;
					}
				}
			}
			//左右均属性
			else  if (contmp->bLhsIsAttr == 1 && contmp->bRhsIsAttr == 1) {
				for (int y = 0; y < allattrcount; y++, ctmpleft++) {
					if ((strcmp(ctmpleft->tabName, contmp->lhsAttr.relName) == 0)
						&& (strcmp(ctmpleft->attrName, contmp->lhsAttr.attrName) == 0)) {
						break;
					}
					if (y == allattrcount - 1) {
						attention = strcat(contmp->lhsAttr.relName, ".");
						attention = strcat(attention, contmp->lhsAttr.attrName);
						attention = strcat(attention, "\r\n不存在，请检查sql语句");
						AfxMessageBox(attention);
						return FAIL;
					}
				}
				for (int y = 0; y < allattrcount; y++, ctmpright++) {
					if ((strcmp(ctmpright->tabName, contmp->rhsAttr.relName) == 0)
						&& (strcmp(ctmpright->attrName, contmp->rhsAttr.attrName) == 0)) {
						break;
					}
					if (y == allattrcount - 1) {
						attention = strcat(contmp->rhsAttr.relName, ".");
						attention = strcat(attention, contmp->rhsAttr.attrName);
						attention = strcat(attention, "\r\n不存在，请检查sql语句");
						AfxMessageBox(attention);
						return FAIL;
					}
				}
				//对conditions的某一个条件进行判断
				for (int z = 0; z < nRelations; z++) {//找到这个条件的属性对应的表在relations数组中的下标位置    
					if (strcmp(relations[z], ctmpleft->tabName) == 0) {
						//对conditions的某一个条件进行判断
						if (ctmpleft->attrType == ints) {//判定属性的类型
							attrtype = ints;
							memcpy(&intleft, (recdkr + z)->pData + ctmpleft->attrOffset, sizeof(int));
						}
						else if (ctmpleft->attrType == chars) {
							attrtype = chars;
							charleft = (char *)malloc(ctmpleft->attrLength);
							memcpy(charleft, (recdkr + z)->pData + ctmpleft->attrOffset, ctmpleft->attrLength);
						}
						else {
							attrtype = floats;
							memcpy(&floatleft, (recdkr + z)->pData + ctmpleft->attrOffset, sizeof(float));
						}
					}
					if (strcmp(relations[z], ctmpright->tabName) == 0) {
						//对conditions的某一个条件进行判断
						if (ctmpright->attrType == ints) {//判定属性的类型
							memcpy(&intright, (recdkr + z)->pData + ctmpright->attrOffset, sizeof(int));
						}
						else if (ctmpright->attrType == chars) {
							charright = (char *)malloc(ctmpright->attrLength);
							memcpy(charright, (recdkr + z)->pData + ctmpright->attrOffset, ctmpright->attrLength);
						}
						else {
							memcpy(&floatright, (recdkr + z)->pData + ctmpright->attrOffset, sizeof(float));
						}
					}
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
			resultcount++;
			for (int z = 0; z < nSelAttrs; z++) {
				if (pr[z].col.attrType == ints) {
					memcpy(&intvalue, (recdkr + pr[z].i)->pData + pr[z].col.attrOffset, sizeof(int));
					charvalue = (char *)malloc(20);
					itoa(intvalue, charvalue, 10);
					memcpy(data + z * 20, charvalue, 20);
					free(charvalue);
				}
				else if (pr[z].col.attrType == chars) {
					memcpy(data + z * 20, (recdkr + pr[z].i)->pData + pr[z].col.attrOffset, 20);
				}
				else {
					memcpy(&floatvalue, (recdkr + pr[z].i)->pData + pr[z].col.attrOffset, sizeof(int));
					sprintf(data + z * 20, "%f", floatvalue);
				}
			}
			data += 20 * nSelAttrs;
		}
		handle();
	}
	resultcount++;//因为还要存储一行表头，所以此处要++
	charvalue = (char *)malloc(20);
	itoa(resultcount, charvalue, 10);
	memcpy(res + 20, charvalue, 20);
	free(charvalue);

	// 释放内存，关闭文件句柄
	free(column);
	free(pr);
	free(attrcount);
	free(counter);
	free(recdkr);
	free(relscan);
	rc = RM_CloseFile(rm_table);
	if (rc != SUCCESS)
		return rc;
	free(rm_table);
	rc = RM_CloseFile(rm_column);
	if (rc != SUCCESS)
		return rc;
	free(rm_column);
	for (int i = 0; i < nRelations; i++) {
		rc = RM_CloseFile(rm_data + i);
		if (rc != SUCCESS)
			return rc;
	}
	free(rm_data);
	return SUCCESS;
}

void handle() {
	counter[counterIndex]++;
	if (counter[counterIndex] >= _recordcount[counterIndex]) {
		counter[counterIndex] = 0;
		counterIndex--;
		if (counterIndex >= 0) {
			handle();
		}
		counterIndex = tablecount - 1;
	}
}