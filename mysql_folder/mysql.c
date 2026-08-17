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