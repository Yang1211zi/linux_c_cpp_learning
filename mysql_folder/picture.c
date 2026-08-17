#include<stdio.h>
#include<mysql/mysql.h>
#include<string.h>
#define mysql_password "1017"
#define mysql_learning_db_ip "localhost"
#define mysql_learning_db_port 3306
#define mysql_database_default "learning_db"
#define mysql_username "db"

#define mysql_insert "insert into user_tl (u_name,u_gender) values('yzy','man')"
#define mysql_select "select* from user_tl"
#define mysql_delete "call delete_name('king')"

int read_image(char*filename,char*buffer){
    if(filename==NULL||buffer==NULL){
       return -1;
    }
    FILE*fp=fopen(filename,"rb");
    if(fp==NULL){
        return -2;
    }
    fseek(fp,0,SEEK_END);
    int length=ftell(fp);
    fseek(fp,0,SEEK_SET);
    int size=fread(buffer,1,length,fp);
    if(size!=length){
        return -3;
    }
    fclose(fp);
    return 0;
}

int write_image(char*filename,char*buffer,int length){
    if(filename==NULL||buffer==NULL){
       return -1;
    }
    FILE*fp=fopen(filename,"wb+");
    if(fp==NULL){
        return -2;
    }
    int size=fwrite(buffer,1,length,fp);
    if(size!=length){
        return -3;
    }
    fclose(fp);
    return 0;
}

#define buffer_size 64*1024
#define mysql_insert_image "insert user_tl(u_name,u_gender,u_image) values('sr','woman',?);"
#define mysql_read_image "select u_image from user_tl where u_name='yzy' ;"
int mysql_write(MYSQL*handle,char*buffer,int length){
    if(handle==NULL||buffer==NULL||length<=0){
        return -1;
    }
    MYSQL_STMT*stmt=mysql_stmt_init(handle);
    int ret=mysql_stmt_prepare(stmt,mysql_insert_image,strlen(mysql_insert_image));
    if(ret){
        return -2;
    }
    MYSQL_BIND param={0};
    param.buffer_type=MYSQL_TYPE_LONG_BLOB;
    param.buffer=NULL;
    param.is_null=0;
    param.length=NULL;
    ret=mysql_stmt_bind_param(stmt,&param);
    if(ret){
        return -3;
    }
    ret=mysql_stmt_send_long_data(stmt,0,buffer,length);
    //这一步只是将数据给到数据库服务器中，并没有与目标数据库建立连接
    if(ret)return -4;
    ret=mysql_stmt_execute(stmt);
    //将数据给到目标数据库中
    if(ret)return -5;
    ret=mysql_stmt_close(stmt);
    if(ret)return -6;
    return 0;
}

int mysql_read(MYSQL*handle,char*buffer,int length){
    if(handle==NULL||buffer==NULL||length<=0){
        return -1;
    }
    MYSQL_STMT*stmt=mysql_stmt_init(handle);
    int ret=mysql_stmt_prepare(stmt,mysql_read_image,strlen(mysql_read_image));
    if(ret){
        return -2;
    }
     MYSQL_BIND result={0};//把result中的所有成员初始化为0或NULL
    result.buffer_type=MYSQL_TYPE_LONG_BLOB;
    unsigned long tol_length=0;
    result.length=&tol_length;
    

    ret=mysql_stmt_bind_result(stmt,&result);

    ret=mysql_stmt_execute(stmt);

    ret=mysql_stmt_store_result(stmt);
    
    while(1){
        
        ret=mysql_stmt_fetch(stmt);//只取图片数据长度
        
        if(ret!=0&&ret!=MYSQL_DATA_TRUNCATED)
            {
        break;}
        int start=0;
        while(start<tol_length){
            result.buffer=buffer+start;//这里才进行真正的存储
            result.buffer_length=1;
            mysql_stmt_fetch_column(stmt,&result,0,start);
            start+=result.buffer_length;
        }
    }
        mysql_stmt_close(stmt);
        return tol_length;
}

int sql_select(MYSQL*mysqls){
    //ns->mysql server
    if(mysql_real_query(mysqls,mysql_select,strlen(mysql_select)))//success->0
    {
        printf("mysql query error:%s\n",mysql_error(mysqls));
        return -1;
    }
    //mysql server->ns(give result to ns)
    MYSQL_RES*res=mysql_store_result(mysqls);
    if(res==NULL){
        printf("mysql_store_result error:%s\n",mysql_error(mysqls));
        return -1;
    }
    //all row and field coount
    int row=mysql_num_rows(res);
    printf("The num of rows is %d\n",row);
    int field=mysql_num_fields(res);
    printf("The num of field is %d\n",field);
    //specific content of every row
    MYSQL_ROW rows;
    while(rows=mysql_fetch_row(res)){
        int i=0;
        for(i=0;i<field;i++){
            printf("%s\t",rows[i]);
        }
        printf("\n");
    }
    mysql_free_result(res);
    return 0;
}

int main(){
    MYSQL mysql;
    if(mysql_init(&mysql)==NULL)//success->1
    {
        printf("mysql init error:%s\n",mysql_error(&mysql));
        return -1;
    }
    if(!mysql_real_connect(&mysql,mysql_learning_db_ip,mysql_username,mysql_password,mysql_database_default,mysql_learning_db_port,NULL,0))
    {
        printf("mysql connection error:%s\n",mysql_error(&mysql));
        return -1;
    }

    sql_select(&mysql);
    #if 0
    if(mysql_real_query(&mysql,mysql_insert,strlen(mysql_insert)))//success->0
    {
        printf("mysql query error:%s\n",mysql_error(&mysql));
        return -1;
    }
    #endif

    printf("************\n");
    #if 1
    if(mysql_real_query(&mysql,mysql_delete,strlen(mysql_delete)))//success->0
    {
        printf("mysql query error:%s\n",mysql_error(&mysql));
        return -1;
    }
    #endif

    sql_select(&mysql);
    mysql_close(&mysql);


    return 0;
}