#include <stdio.h>
#include <string.h>
#define out 0
#define in 1
#define init out

int judge_word(char c){
    if((c==' ')||(c=='\n')||(c=='\t')||(c=='+')||(c=='-')||(c=='*')||(c=='/')
    ||(c=='=')||(c==';')||(c==',')||(c=='(')||(c==')')||(c=='{')||(c=='}')||(c=='-')){
        return 0;
    }
    else{
        return 1;
    }
}

int count_txt(char*file){
    int word = 0;
    int state = init;
    FILE *fp = fopen(file, "r");
    if(fp==NULL){
        printf("file is not exist\n");
        return -1;
    }
    char c  =  '0';
    while((c=fgetc(fp))!=EOF){
        if(judge_word(c)==0){
            state = out;
        }
        else if(state==out){
            state = in;
            word++;
        }
    }
    fclose(fp);
    return word;
}

int count_word(char*file){
    int state = init;
    int length=count_txt(file);
    char word[length+1][30];
    memset(word,0,sizeof(word));
    char temp[20]={0};
    char c='0';
    int i,j=0;
    int number=0;
    FILE *fp = fopen(file, "r");
    if(fp==NULL){
        printf("file is not exist\n");
        return -1;
    }
    while((c=fgetc(fp))!=EOF){
        if(judge_word(c)==0){
            state = out;
        }
        else if(state==out){
            temp[i]=c;
            state = in;
            i++;
        }
        else if(state==in){
            strcpy(word[j], temp);
            memset(temp,0,sizeof(temp));
            for(int g=1;(g<j&&j>0);g++){
            if(strcmp(word[j],word[g])==0){
              number++;
            }
            }
            j++;
        }
        
    }
    return number;
}

int main(int argc,char *argv[])//主函数带参数时，
{
    if(argc<2){
        printf("please input file name\n");
        return -1;
    }
    int word=count_txt(argv[1]);
    printf("word count is %d\n",word);
    return 0;
}