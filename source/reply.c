/* ehttpd Lari Korpi lari.korpi@iki.fi (c) 2006 */

#include <stdio.h>
#include <string.h>
#include <http.h>
#include <reply.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <regex.h>

int html_menu(http_request req, char *page) {

  char cwd[]=".";

  DIR *working_directory;

  struct dirent * working_directory_ent;

  struct stat file_stat;

  if(!stat(strcat(cwd,req.target),&file_stat)) {
    if(!S_ISDIR(file_stat.st_mode)) {
      page[0]=0;
      return 0;
    }
    working_directory=opendir(strcat(cwd,req.target));
  } else {
    page[0]=0;
    return 0; 
  }

  sprintf(page,"<html>\n");
  sprintf(page+strlen(page),"<head>\n");
  sprintf(page+strlen(page),"<title>%s</title>\n",req.target);
  sprintf(page+strlen(page),"</head>\n");
  sprintf(page+strlen(page),"<body>\n");
  sprintf(page+strlen(page),"<h2>%s</h2>\n",req.target);
  sprintf(page+strlen(page),"<ul>\n");

  working_directory_ent=readdir(working_directory);
  while (working_directory_ent != NULL) {
    if (strncmp(working_directory_ent->d_name,".",1)) {
      stat(working_directory_ent->d_name,&file_stat);
      if(S_ISREG(file_stat.st_mode)) {
	sprintf(page+strlen(page),"<li><b>F</b>:<a href='/choice?edit=%s",req.target);
	if (strcmp(req.target,"/")) {
	  sprintf(page+strlen(page),"/");
	}
	sprintf(page+strlen(page),"%s'>%s</a></li>\n",working_directory_ent->d_name,working_directory_ent->d_name);
      } else if (S_ISDIR(file_stat.st_mode)){
	sprintf(page+strlen(page),"<li><b>B</b>:<a href='%s",req.target);
	if (strcmp(req.target,"/")) {
	  sprintf(page+strlen(page),"/");
	}
	sprintf(page+strlen(page),"%s'>%s</a></li>\n",working_directory_ent->d_name,working_directory_ent->d_name);
      } else if (S_ISLNK(file_stat.st_mode)) {
	sprintf(page+strlen(page),"<li><b>L</b>:%s</li>\n",working_directory_ent->d_name);
      } else {
	sprintf(page+strlen(page),"<li>Could not stat %s</li>\n",working_directory_ent->d_name);
      }
    }
    working_directory_ent=readdir(working_directory);
    
  }

  sprintf(page+strlen(page),"</ul>\n");
  sprintf(page+strlen(page),"<a href='/choice?new=%s'>Add new file</a>",req.target);
  sprintf(page+strlen(page),"</body>\n");
  sprintf(page+strlen(page),"</html>\n");
  
  return 1;
}


int html_choice(http_request req, char *page) {

  int i=0,ii=0,line=-2;

  char file_path[1024]={0};
  char buffer1[1024]={0};
  char buffer2[1024]={0};
  char buffer3[1024]={0};
  char cwd[]=".";
 
  FILE *fd=0,*fd_tmp=0;

  regex_t get_method;
  
  regmatch_t pm[64]={0};
  
  while(req.target[i]!='\0') {
    if(req.target[i]=='%') {
      char tmp_buf[5]={0};
      tmp_buf[0] = '0';
      tmp_buf[1] = 'x';
      tmp_buf[2] = req.target[i+1];
      tmp_buf[3] = req.target[i+2];
      buffer1[ii] = (char)strtol(tmp_buf,NULL,16);
      i=i+2;
    } else if (req.target[i]=='+') { 
      buffer1[ii] = ' ';
    } else {
      buffer1[ii] = req.target[i];
    }
    ii++;
    i++;
  }
  i=0;

  strcpy(req.target,buffer1);

  regcomp(&get_method, "\\?" ,REG_EXTENDED);
  regexec(&get_method, req.target, (size_t)strlen(req.target), pm, 0);
  regfree(&get_method);

  strcpy(file_path,&req.target[pm[0].rm_so+6]);

  sprintf(page,"<html>\n");
  sprintf(page+strlen(page),"<head>\n");
  sprintf(page+strlen(page),"<title>/choice</title>\n");
  sprintf(page+strlen(page),"<body>\n");
  sprintf(page+strlen(page),"<h2>%s</h2>\n",req.target);

  sprintf(page+strlen(page),"<!-- buffer1:%s -->",buffer1);
 
  regcomp(&get_method, "&" ,REG_EXTENDED);
  if(regexec(&get_method, file_path, (size_t)strlen(file_path), pm, 0)!=REG_NOMATCH) {
    strcpy(buffer2,&file_path[pm[0].rm_so+6]);
    line=atoi(buffer2);
    sprintf(page+strlen(page),"<!-- line1:%d -->",line);  
    sprintf(page+strlen(page),"<!-- buffer2:%s -->",buffer2);
    file_path[pm[0].rm_so]='\0';
    if(regexec(&get_method, buffer2, (size_t)strlen(buffer2), pm, 0)!=REG_NOMATCH) {
      int line2=0;
      strcpy(buffer3,&buffer2[pm[0].rm_so+6]);
      buffer2[pm[0].rm_so]='\0';
      line2=atoi(buffer2);
      sprintf(page+strlen(page),"<!-- line2:%d -->",line2); 
      sprintf(page+strlen(page),"<!-- buffer3:%s -->",buffer3);
    }
  }
  regfree(&get_method);


  if(line>=0 && strlen(buffer3)==0) {
    sprintf(page+strlen(page),"<p>Action edit line %d of %s</p>\n<p>\n",line,file_path);
  } else if (strlen(buffer3)!=0) {
    sprintf(page+strlen(page),"<p>Action write <b>%s</b> to line <b>%d</b> of <b>%s</b></p>\n<p>\n",buffer3,line,file_path);
    sprintf(page+strlen(page),"View <a href='/choice?edit=%s'>%s</a></p>\n",file_path,file_path);
  } else {
    sprintf(page+strlen(page),"<p>Action view %s</p>\n<p><a href='%s&line=%d'>%d</a>: \n",file_path,req.target,i,i);
    i++;
  }

  if(line==0 && strlen(buffer3)==0) {
    sprintf(page+strlen(page),"<form action='%s' method='get'>",req.target);
    sprintf(page+strlen(page),"<input type='hidden' name='edit' value='%s'><input type='hidden' name='line' value='%d'>",file_path,line);
    sprintf(page+strlen(page),"<input type='text' size='80' name='repl' value='");
  }
  
  if(strlen(buffer3)!=0) {
    fd=fopen(strcat(cwd,file_path),"r+");
    fd_tmp=tmpfile();
    if(fd!=NULL && fd_tmp!=NULL) {
      
      while(feof(fd)==0 && line!=i) { 
	char tmp_buf=0;
	tmp_buf=fgetc(fd);
	fputc(tmp_buf,fd_tmp);
	if (tmp_buf=='\n') {
	  i++;
	}
      }
      
      fprintf(fd_tmp,"%s\n",buffer3);

      while(feof(fd)==0 && line==i) { 
	char tmp_buf=0;
	tmp_buf=fgetc(fd);	
	if (tmp_buf=='\n') {
	  i++;
	}
      }

      while(feof(fd)==0) { 
	char tmp_buf=0;
	tmp_buf=fgetc(fd);
	fputc(tmp_buf,fd_tmp);
      }

      fflush(fd);
      fflush(fd_tmp);
      rewind(fd);
      rewind(fd_tmp);

      while(feof(fd_tmp)==0) {
	char tmp_buf=0;
	tmp_buf=fgetc(fd_tmp);
	fputc(tmp_buf,fd);
      }

      fclose(fd_tmp);
      fclose(fd);
    }
    i=0;
   } else {

    fd=fopen(strcat(cwd,file_path),"r");
    
    if(fd!=NULL) {
      while(feof(fd)==0) { 
	char tmp_buf=0;
	tmp_buf=fgetc(fd);
	if (tmp_buf=='\n') {
	  if(line-1==i) {
	    sprintf(page+strlen(page),"<form action='%s' method='get'>\n",req.target);
	    sprintf(page+strlen(page),"<input type='hidden' name='edit' value='%s'><input type='hidden' name='line' value='%d'>",file_path,line);
	    sprintf(page+strlen(page),"<input type='text' size='80' name='repl' value='");
	  } else if(line==i) {
	    sprintf(page+strlen(page),"'></br><input type='submit' value='write to line %d'>\n",line);
	    sprintf(page+strlen(page),"</form>\n");
	  } else if (line<0){
	    sprintf(page+strlen(page),"</br>\n<a href='%s&line=%d'>%d</a>: ",req.target,i,i);
	  }
	  i++;
	}
	if((line==i || line<0) && tmp_buf!='\n'){
	  sprintf(page+strlen(page),"%c",tmp_buf);
	}
      }
      fclose(fd);
    } else {
      sprintf(page+strlen(page),"Unable to open file %s.",file_path);
    }
   }
  sprintf(page+strlen(page),"</p>\n</body>\n");
  sprintf(page+strlen(page),"</html>\n");
  return 1;
}
