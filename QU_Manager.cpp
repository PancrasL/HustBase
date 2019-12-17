#include "StdAfx.h"
#include "QU_Manager.h"
#include "RM_Manager.h"

//select���ܵ�ȫ�־�̬����
static int *_recordcount;//�������ݱ�����ļ�¼����
static int tablecount; //����
static int counterIndex;
static int *counter;
typedef struct printrel {
	int i;	//�������Զ�Ӧ������Ӧ���±�
	SysColumn col;//����������Ϣ
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
	rc = parse(sql, sql_str);//ֻ�����ַ��ؽ��SUCCESS��SQL_SYNTAX
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
	RM_Record *recdkr;//����һ���ѿ�����¼
	SysColumn *column, *ctmp, *ctmpleft, *ctmpright;
	Condition *contmp;
	int allattrcount = 0;//�漰��nRelations�����ݱ����������֮��
	int *attrcount;//�������ݱ��������������
	int allcount = 1;//�ѿ�����������
	int torf;//�Ƿ����ɾ������
	int resultcount = 0;//�����
	int intvalue, intleft, intright;
	char *charvalue, *charleft, *charright;
	float floatvalue, floatleft, floatright;
	RC rc;
	printrel *pr;
	char *res;
	AttrType attrtype;
	char *attention;

	pr = (printrel*)malloc(nSelAttrs * sizeof(printrel));
	//�����ݱ�,ϵͳ��ϵͳ���ļ�
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
	//��nRelations�����ݱ��������ļ������rm_data + i��
	rm_data = (RM_FileHandle *)malloc(nRelations * sizeof(RM_FileHandle));
	for (int i = 0; i < nRelations; i++) {
		(rm_data + i)->bOpen = false;
		rc = RM_OpenFile(relations[i], rm_data + i);
		if (rc != SUCCESS)
			return rc;
	}//ע��˳�� rm_data -> relations   : 0-(nRelations-1)  -> 0-(nRelations-1)

	//��ȡϵͳ����ȡ����������Ϣ��
	//������������_allattrcount,������ϵ����������������*��_attrcount + i ����
	attrcount = (int *)malloc(nRelations * sizeof(int));
	//�ҵ�����������Ӧ��������������������_attrcount������
	for (int i = 0; i < nRelations; i++) {
		FileScan.bOpen = false;
		rc = OpenScan(&FileScan, rm_table, 0, NULL);//ÿ�β��ҳ�ʼ����
		if (rc != SUCCESS)
			return rc;
		while (GetNextRec(&FileScan, &rectab) == SUCCESS) {
			if (strcmp(relations[i], rectab.pData) == 0) {
				memcpy(attrcount + i, rectab.pData + 21, sizeof(int));
				allattrcount += *(attrcount + i);//�������漰��ϵ�����Ե��ܺͣ�������_allattrcount��
				break;
			}
		}
	}//ע��˳��_attrcount -> relations : 0-(nRelations-1) -> 0-(nRelations-1)
	//��ȡϵͳ�У���ȡ�漰���ݱ��ȫ��������Ϣ
	//��������columnΪ��ʼ��ַ�Ŀռ��У�˳���ǰ���relations������˳��
	column = (SysColumn *)malloc(allattrcount * sizeof(SysColumn));//����_allattrcount��Syscolumn�ռ䣬���ڱ���nRealtions����ϵ������
	ctmp = column;
	for (int i = 0; i < nRelations; i++) {
		FileScan.bOpen = false;
		rc = OpenScan(&FileScan, rm_column, 0, NULL);
		if (rc != SUCCESS)
			return rc;//����ÿ�����������Ϣ��Ҫ��ʼ��ɨ��
		while (GetNextRec(&FileScan, &reccol) == SUCCESS) {
			if (strcmp(relations[i], reccol.pData) == 0) {//�ҵ�����Ϊ*��relations + i���ĵ�һ����¼
				for (int j = 0; j < *(attrcount + i); j++) {  //���ζ�ȡ*��_attrcount + i��������
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
	//��ȡselAttrs��Ӧ�ĵѿ�������λ��

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
	//��ȡ���ݱ���ȡ��¼��
	//������recordcount�У�˳���ǰ��ձ���relations��˳��
	_recordcount = (int *)malloc(nRelations * sizeof(int));
	for (int i = 0; i < nRelations; i++) {
		FileScan.bOpen = false;
		rc = OpenScan(&FileScan, rm_data + i, 0, NULL);
		if (rc != SUCCESS)
			return rc;//����ÿ�����������Ϣ��Ҫ��ʼ��ɨ��
		*(_recordcount + i) = 0;
		while (GetNextRec(&FileScan, &recdata) == SUCCESS) {
			*(_recordcount + i) += 1;
		}
	}
	//��ȡ����Ϣ���������ݱ����������������ݱ�������Ϣ�������ݱ�ļ�¼��
	//��һ���ַ������������������ڶ����ַ��������¼������������nSelAttrs���ֽڱ���������
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
	//ͨ���ѿ����㷨��ʵ�ָ�����ϵ������
	counter = (int *)malloc(nRelations * sizeof(int));
	for (int i = 0; i < nRelations; i++) {
		counter[i] = 0;
	}
	counterIndex = nRelations - 1;
	tablecount = nRelations;
	relscan = (RM_FileScan *)malloc(nRelations * sizeof(RM_FileScan));
	recdkr = (RM_Record *)malloc(nRelations * sizeof(RM_Record));
	//��nRelations�����ݱ�ֱ��ʼ��ɨ�裬������ѿ���������Ŀ��
	for (int i = 0; i < nRelations; i++) {
		allcount *= _recordcount[i];
		(relscan + i)->bOpen = false;
		rc = OpenScan(relscan + i, rm_data + i, 0, NULL);
		if (rc != SUCCESS)
			return rc;
	}

	for (int i = 0; i < allcount; i++) {
		//ȡ��һ���ѿ�����¼��һ���ѿ�����¼��ӦnRelations�����е�һ����¼
		//ȡ��relations +j�����ݱ�ĵĵ�counter[j]����¼������recdkr + j��
		//����һ����¼֮��Ҫ��relscan ���³�ʼ��ɨ��
		for (int j = 0; j < nRelations; j++) {
			for (int k = 0; k != counter[j] + 1; k++) {
				GetNextRec(relscan + j, recdkr + j);
			}
			(relscan + j)->bOpen = false;
			rc = OpenScan(relscan + j, rm_data + j, 0, NULL);
			if (rc != SUCCESS)
				return rc;
		}
		//�����ж�
		torf = 1;
		contmp = conditions;
		for (int x = 0; x < nConditions; x++, contmp++) {//��nSelAttrs��������һ�ж�
			ctmpleft = ctmpright = column;//��ctmp���õ�column��
			//��������ֵ
			if (contmp->bLhsIsAttr == 1 && contmp->bRhsIsAttr == 0) {
				for (int y = 0; y < allattrcount; y++, ctmpleft++) {//��attrcount�������ҵ���Ӧ��conditions��������������Ӧ�ļ�¼
					if ((strcmp(ctmpleft->tabName, contmp->lhsAttr.relName) == 0)//�ҵ������е����Զ�Ӧϵͳ���е����������Ϣ
						&& (strcmp(ctmpleft->attrName, contmp->lhsAttr.attrName) == 0)) {
						break;
					}
					if (y == allattrcount - 1) {
						attention = strcat(contmp->lhsAttr.relName, ".");
						attention = strcat(attention, contmp->lhsAttr.attrName);
						attention = strcat(attention, "\r\n�����ڣ�����sql���");
						AfxMessageBox(attention);
						return FAIL;
					}
				}

				for (int z = 0; z < nRelations; z++) {//�ҵ�������������Զ�Ӧ�ı���relations�����е��±�λ��    
					if (strcmp(relations[z], ctmpleft->tabName) == 0) {
						//��conditions��ĳһ�����������ж�
						if (ctmpleft->attrType == ints) {//�ж����Ե�����
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

			//��������ֵ
			else  if (contmp->bLhsIsAttr == 0 && contmp->bRhsIsAttr == 1) {
				for (int y = 0; y < allattrcount; y++, ctmpright++) {//attrcount��������һ�ж�
					if ((strcmp(ctmpright->tabName, contmp->rhsAttr.relName) == 0)
						&& (strcmp(ctmpright->attrName, contmp->rhsAttr.attrName) == 0)) {//���ݱ����������ҵ���Ӧ����
						break;
					}
					if (y == allattrcount - 1) {
						attention = strcat(contmp->rhsAttr.relName, ".");
						attention = strcat(attention, contmp->rhsAttr.attrName);
						attention = strcat(attention, "\r\n�����ڣ�����sql���");
						AfxMessageBox(attention);
						return FAIL;
					}
				}
				for (int z = 0; z < nRelations; z++) {//�ҵ�������������Զ�Ӧ�ı���relations�����е��±�λ��    
					if (strcmp(relations[z], ctmpleft->tabName) == 0) {
						//��conditions��ĳһ�����������ж�
						if (ctmpright->attrType == ints) {//�ж����Ե�����
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
			//���Ҿ�����
			else  if (contmp->bLhsIsAttr == 1 && contmp->bRhsIsAttr == 1) {
				for (int y = 0; y < allattrcount; y++, ctmpleft++) {
					if ((strcmp(ctmpleft->tabName, contmp->lhsAttr.relName) == 0)
						&& (strcmp(ctmpleft->attrName, contmp->lhsAttr.attrName) == 0)) {
						break;
					}
					if (y == allattrcount - 1) {
						attention = strcat(contmp->lhsAttr.relName, ".");
						attention = strcat(attention, contmp->lhsAttr.attrName);
						attention = strcat(attention, "\r\n�����ڣ�����sql���");
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
						attention = strcat(attention, "\r\n�����ڣ�����sql���");
						AfxMessageBox(attention);
						return FAIL;
					}
				}
				//��conditions��ĳһ�����������ж�
				for (int z = 0; z < nRelations; z++) {//�ҵ�������������Զ�Ӧ�ı���relations�����е��±�λ��    
					if (strcmp(relations[z], ctmpleft->tabName) == 0) {
						//��conditions��ĳһ�����������ж�
						if (ctmpleft->attrType == ints) {//�ж����Ե�����
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
						//��conditions��ĳһ�����������ж�
						if (ctmpright->attrType == ints) {//�ж����Ե�����
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
	resultcount++;//��Ϊ��Ҫ�洢һ�б�ͷ�����Դ˴�Ҫ++
	charvalue = (char *)malloc(20);
	itoa(resultcount, charvalue, 10);
	memcpy(res + 20, charvalue, 20);
	free(charvalue);

	// �ͷ��ڴ棬�ر��ļ����
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