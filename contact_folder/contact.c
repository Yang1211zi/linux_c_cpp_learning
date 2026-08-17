#include<stdio.h>
#include<string.h>
#include <stdlib.h>

#define name_length 20
#define phone_length 12
#define buffer_size 1024
#define List_insert(item,list) do{   \
    item->next=list;  \
    item->prev=NULL;  \
    if(list!=NULL){\
        list->prev=item;\
    }\
    list=item;\
}while(0)

#define List_remove(item,list)do{\
    if(item->next!=NULL){\
        item->next->prev=item->prev;\
    }\
    if(item->prev!=NULL){\
        item->prev->next=item->next;\
    }\
    else{\
        list=item->next;\
    }\
    item->next=NULL;\
    item->prev=NULL;\
}while(0)

enum {
    CONTACT_INSERT=1,
    CONTACT_REMOVE,
    CONTACT_SEARCH,
    CONTACT_PRINT,
    FILE_SAVE,
    FILE_LOAD
};

typedef struct person{
    char name[name_length];
    char phone[phone_length];

    struct person *next;
    struct person *prev;
} person;

typedef struct contact{
    int count;
    struct person *people;
}contact;

person *contact_search(person *ps, char *name){
    person *item=NULL;
    for(item=ps;item!=NULL;item=item->next){
        if(!strcmp(item->name,name))
        return item;
    }
}

int contact_travel(person *ps){
    person*item=NULL;
    for(item=ps;item!=NULL;item=item->next){
        printf("name:%s,phone:%s\n",item->name,item->phone);
    }
    return 0;
}

int contact_insert(person*people,person**head){
    if(head==NULL)
    return -1;
    List_insert(people,(*head));
}

int contact_remove(person*people,person**head){
    if(head==NULL)
    return -1;
    List_remove(people,(*head));
}

void contact_meun() {
    printf("please input number for contact\n");
    printf("1:insert\n");
    printf("2:remove\n");
    printf("3:search\n");
    printf("4:print\n");
    printf("5:save\n");
    printf("6:load\n");
}

int contact_save(person*cta,const char *f) {
    FILE*fp=fopen(f,"w");
    if(cta==NULL) {
        printf("contact is empty");
        return -1;
    }
    person*c=malloc(sizeof(person));
    memset(c,0,sizeof(person));
    for (c=cta;c!=NULL;c=c->next) {
        fprintf(fp,"%s,%s\n",c->name,c->phone);
    }
    fflush(fp);
    if (fflush(fp)==EOF) {
        printf("file write error\n");
    }
    printf("save success\n");
    printf("               \n");
    fclose(fp);
}

int FILE_MSG (const char *buf,int length,char*name,char*phone) {
    if (buf==NULL) {
        return -1;
    }
    int state=0;
    int j=0;
    int i=0;
    for ( i=0; buf[i]!= ',' ;i++) {
            name[j++]=buf[i];
    }
    state=0;
    j=0;
    i=i+1;
    for (;i<length;i++) {
            phone[j++]=buf[i];
    }
    return 0;
}

int contact_load(contact*cta,const char *f) {
    FILE*fp=fopen(f,"r");
    if(cta==NULL) {
        printf("contact is empty\n");
        return -1;
    }
    if (fp==NULL) {
        printf("file is empty or you have not save\n");
        return -1;
    }
    char buffer[buffer_size]={0};
    char temp_name[name_length]={0};
    char temp_phone[name_length]={0};
    while (fgets(buffer,buffer_size,fp)!=NULL) {
        int len=strlen(buffer);
        if (FILE_MSG(buffer,len,temp_name,temp_phone)!=0) {
            continue;
        }
        person*c=malloc(sizeof(person));
        if (c==NULL) {
            return -1;
        }
        memcpy(c->name,temp_name,name_length);
        memcpy(c->phone,temp_phone,phone_length);
        contact_insert(c,&cta->people);
        cta->count++;
        printf("load success\n");
    }
}

int INSERT (contact  *c){
    person*p=malloc(sizeof(person));
    if(p==NULL)
    return -1;
    printf("please input name:");
    scanf("%s",p->name);
    printf("please input phone:");
    scanf("%s",p->phone);
    int temp=contact_insert(p,&c->people);
    if(temp==-1)
    {
        printf("list is empty, insert failed\n");
        free(p);
        return -1;
   }
    c->count++;
    printf("insert success\n");
    printf("               \n");
    return 0;
}

int PTINT (contact *c){
    if(c==NULL) {
        printf("contact is NULL\n");
        return -1;
    }
    contact_travel(c->people);
    printf("              \n");
    return 0;
}

int SEARCH (contact*c){
    if(c==NULL){
        return -1;
        printf("contact is empty");
    }
    char temp_name[name_length]={0};
    printf("please input name for search:");
    scanf("%s",temp_name);
    person*p=contact_search(c->people,temp_name);
    if(p==NULL) printf("name wrong or the people is not exist\n");
    else printf("name:%s,phone:%s\n",p->name,p->phone);
    printf("                               \n");
    return 0;
}

int DELETE (contact*c){
    if(c==NULL){
        printf("contact is empty");
        return -1;
    }
    char temp_name[name_length]={0};
    printf("please input name for delete:");
    scanf("%s",temp_name);
    person*p=contact_search(c->people,temp_name);
    if(p==NULL) printf("name wrong or the people is not exist");
    contact_remove(p,&c->people);
    free(p);
    printf("delete success");
    printf("                                 \n");
    c->count--;

    return 0;
}

int SAVE(contact*cta) {
    if(cta==NULL) {
        printf("contact is NULL\n");
        return -1;
    }
    char filename[name_length]={0};
    printf("please input the file you want to save:");
    scanf("%s",filename);
    contact_save(cta->people,filename);
}

int LOAD (contact*cta) {
    if(cta==NULL) {
        printf("contact is NULL\n");
        return -1;
    }
    char filename[name_length]={0};
    printf("please input the file you want to load:");
    scanf("%s",filename);
    contact_load(cta,filename);
    return 0;
}

int main(){
    contact*cta = malloc(sizeof(contact));
    if(cta==NULL){
        printf("malloc failed");
        return -1;
    }
    memset(cta,0,sizeof(contact));
while(1){
    contact_meun();
    int select=0;
    scanf("%d",&select);
    switch(select){
        case CONTACT_INSERT:
            INSERT(cta);
            break;
        case CONTACT_REMOVE:
            DELETE(cta);
            break;
        case CONTACT_SEARCH:
            SEARCH(cta);
            break;
        case CONTACT_PRINT:
            PTINT(cta);
            break;
        case FILE_SAVE:
            SAVE(cta);
            break;
        case FILE_LOAD:
            LOAD(cta);
            break;
    }
}
}//
// Created by camel on 2026/7/16.
//