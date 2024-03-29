#ifndef RC_HH
#define RC_HH
typedef enum {
	SUCCESS,
	SQL_SYNTAX,
	PF_EXIST,
	PF_FILEERR,
	PF_INVALIDNAME,
	PF_WINDOWS,
	PF_FHCLOSED,
	PF_FHOPEN,
	PF_PHCLOSED,
	PF_PHOPEN,
	PF_NOBUF,
	PF_EOF,
	PF_INVALIDPAGENUM,
	PF_NOTINBUF,
	PF_PAGEPINNED,
	RM_FHCLOSED,
	RM_FHOPENNED,
	RM_INVALIDRECSIZE,
	RM_INVALIDRID,
	RM_FSCLOSED,
	RM_NOMORERECINMEM,
	RM_FSOPEN,
	IX_IHOPENNED,
	IX_IHCLOSED,
	IX_INVALIDKEY,
	IX_NOMEM,
	RM_NOMOREIDXINMEM,
	IX_EOF,
	IX_SCANCLOSED,
	IX_ISCLOSED,
	IX_NOMOREIDXINMEM,
	IX_SCANOPENNED,
	FAIL,

	DB_EXIST,
	DB_NOT_EXIST,
	NO_DB_OPENED,

	TABLE_NOT_EXIST,
	TABLE_EXIST,
	TABLE_NAME_ILLEGAL,

	FLIED_NOT_EXIST,//在不存在的字段上增加索引
	FIELD_NAME_ILLEGAL,
	FIELD_MISSING,//插入的时候字段不足
	FIELD_REDUNDAN,//插入的时候字段太多
	FIELD_TYPE_MISMATCH,//字段类型有误

	RECORD_NOT_EXIST,//对一条不存在的记录进行删改时

	INDEX_NAME_REPEAT,
	INDEX_EXIST,//在指定字段上，已经存在索引了
	INDEX_NOT_EXIST,
	INDEX_CREATE_FAILED,
	INDEX_DELETE_FAILED,

	INCORRECT_QUERY_RESULT,    //查询结果错误
	ABNORMAL_EXIT,  //异常退出

	TABLE_CREATE_REPEAT,  //表重复创建
	TABLE_CREATE_FAILED,  //创建表失败
	TABLE_COLUMN_ERROR,    //列数/列名/列类型/列长度错误
	TABLE_ROW_ERROR,
	TABLE_DELETE_FAILED,   //表删除失败

	DATABASE_FAILED //数据库创建或删除失败
}RC;
#define CHECK_RC(rc) if (rc != SUCCESS) {return rc;}
#endif