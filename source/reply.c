/* ehttpd Lari Korpi lari.korpi@iki.fi (c) 2006 */

#include <stdio.h>
#include <string.h>
#include <http.h>
#include <reply.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <regex.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>

#define MAX_EDIT_CONTENT 512

static int build_safe_path(const char *relative_path, char *path, size_t path_size)
{
  const char *part;
  if (relative_path == NULL || relative_path[0] != '/' ||
      strstr(relative_path, "\\") != NULL)
    return -1;
  part = relative_path;
  while ((part = strstr(part, "..")) != NULL) {
    if ((part == relative_path || part[-1] == '/') &&
        (part[2] == '\0' || part[2] == '/')) return -1;
    part += 2;
  }
  return snprintf(path, path_size, ".%s", relative_path) < (int)path_size ? 0 : -1;
}

int html_menu(http_request req, char *page) {

  char full_path[1024];
  char entry_path[2048];

  DIR *working_directory;

  struct dirent * working_directory_ent;

  struct stat file_stat;

  if (build_safe_path(req.target, full_path, sizeof(full_path)) < 0 ||
      stat(full_path, &file_stat) != 0) {
    page[0] = 0;
    return 0;
  }
  if(!stat(full_path,&file_stat)) {
    if(!S_ISDIR(file_stat.st_mode)) {
      page[0]=0;
      return 0;
    }
    working_directory=opendir(full_path);
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
      snprintf(entry_path, sizeof(entry_path), "%s/%s", full_path,
               working_directory_ent->d_name);
      if (stat(entry_path, &file_stat) != 0) {
        working_directory_ent=readdir(working_directory);
        continue;
      }
      if(S_ISREG(file_stat.st_mode)) {
        sprintf(page+strlen(page),"<li><b>F</b>:<a href='%s",req.target);
	if (strcmp(req.target,"/")) {
	  sprintf(page+strlen(page),"/");
	}
        sprintf(page+strlen(page),"%s?action=edit'>%s</a></li>\n",working_directory_ent->d_name,working_directory_ent->d_name);
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
  closedir(working_directory);
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
  char safe_file_path[2048];
 
  FILE *fd=0,*fd_tmp=0;

  regex_t get_method;
  
  regmatch_t pm[64]={0};
  
  while(req.target[i]!='\0' && ii < (int)sizeof(buffer1)-1) {
    if(req.target[i]=='%') {
      if (req.target[i + 1] == '\0' || req.target[i + 2] == '\0' ||
          !isxdigit((unsigned char)req.target[i + 1]) ||
          !isxdigit((unsigned char)req.target[i + 2])) return 0;
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
  if (regexec(&get_method, req.target, 64, pm, 0) != 0) {
    regfree(&get_method);
    snprintf(page, 1024 * 1024,
             "<html><body><h1>Invalid request</h1></body></html>\n");
    return 1;
  }
  regfree(&get_method);

  strcpy(file_path,&req.target[pm[0].rm_so+6]);
  if (build_safe_path(file_path, safe_file_path, sizeof(safe_file_path)) < 0) {
    snprintf(page, 1024 * 1024,
             "<html><body><h1>Invalid path</h1></body></html>\n");
    return 1;
  }

  sprintf(page,"<html>\n");
  sprintf(page+strlen(page),"<head>\n");
  sprintf(page+strlen(page),"<title>/choice</title>\n");
  sprintf(page+strlen(page),"<body>\n");
  sprintf(page+strlen(page),"<h2>%s</h2>\n",req.target);

  sprintf(page+strlen(page),"<!-- buffer1:%s -->",buffer1);
 
  regcomp(&get_method, "&" ,REG_EXTENDED);
  if(regexec(&get_method, file_path, 64, pm, 0)!=REG_NOMATCH) {
    strcpy(buffer2,&file_path[pm[0].rm_so+6]);
    line=atoi(buffer2);
    sprintf(page+strlen(page),"<!-- line1:%d -->",line);  
    sprintf(page+strlen(page),"<!-- buffer2:%s -->",buffer2);
    file_path[pm[0].rm_so]='\0';
    if(regexec(&get_method, buffer2, 64, pm, 0)!=REG_NOMATCH) {
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
    fd=fopen(safe_file_path,"r");
    fd_tmp=tmpfile();
    if(fd!=NULL && fd_tmp!=NULL) {
      
      {
        int tmp_buf;
        while((tmp_buf=fgetc(fd)) != EOF && line!=i) {
	fputc(tmp_buf,fd_tmp);
	if (tmp_buf=='\n') {
	  i++;
	}
        }
      }
      
      fprintf(fd_tmp,"%s\n",buffer3);

      {
        int tmp_buf;
        while((tmp_buf=fgetc(fd)) != EOF && line==i) {
	if (tmp_buf=='\n') {
	  i++;
	}
        }
      }

      {
        int tmp_buf;
        while((tmp_buf=fgetc(fd)) != EOF) {
	fputc(tmp_buf,fd_tmp);
        }
      }

      fflush(fd_tmp);
      rewind(fd);
      rewind(fd_tmp);

      fclose(fd);
      fd=fopen(safe_file_path,"w");
      if (fd != NULL) {
        int tmp_buf;
        while ((tmp_buf=fgetc(fd_tmp)) != EOF) fputc(tmp_buf, fd);
        fclose(fd);
      }
      fclose(fd_tmp);
    }
    i=0;
   } else {

     fd=fopen(safe_file_path,"r");
    
    if(fd!=NULL) {
      {
        int tmp_buf;
        while((tmp_buf=fgetc(fd)) != EOF) {
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

static int action_is_edit(const char *query)
{
  return query != NULL && strcmp(query, "action=edit") == 0;
}

static int decode_url(const char *input, char *out, size_t out_size, int plus_is_space)
{
  size_t offset = 0, output = 0;
  while (input[offset] != '\0' && input[offset] != '&') {
    if (output + 1 >= out_size) return -1;
    if (input[offset] == '+') out[output++] = plus_is_space ? ' ' : '+';
    else if (input[offset] == '%') {
      char hex[3];
      char decoded;
      if (!isxdigit((unsigned char)input[offset + 1]) ||
          !isxdigit((unsigned char)input[offset + 2])) return -1;
      hex[0] = input[offset + 1]; hex[1] = input[offset + 2]; hex[2] = '\0';
      decoded = (char)strtol(hex, NULL, 16);
      if (decoded == '\0') return -1;
      out[output++] = decoded;
      offset += 2;
    } else out[output++] = input[offset];
    offset++;
  }
  out[output] = '\0';
  return 0;
}

static int html_escape(const char *input, char *out, size_t out_size)
{
  size_t used = 0;
  while (*input != '\0') {
    const char *replacement = NULL;
    if (*input == '&') replacement = "&amp;";
    else if (*input == '<') replacement = "&lt;";
    else if (*input == '>') replacement = "&gt;";
    else if (*input == '\"') replacement = "&quot;";
    else if (*input == '\'') replacement = "&#39;";
    if (replacement != NULL) {
      size_t length = strlen(replacement);
      if (used + length >= out_size) return -1;
      memcpy(out + used, replacement, length);
      used += length;
    } else {
      if (used + 1 >= out_size) return -1;
      out[used++] = *input;
    }
    input++;
  }
  out[used] = '\0';
  return 0;
}

static int url_encode_path(const char *input, char *out, size_t out_size)
{
  static const char hex[] = "0123456789ABCDEF";
  size_t used = 0;
  while (*input != '\0') {
    unsigned char character = (unsigned char)*input++;
    if (isalnum(character) || strchr("/-._~", character) != NULL) {
      if (used + 1 >= out_size) return -1;
      out[used++] = (char)character;
    } else {
      if (used + 3 >= out_size) return -1;
      out[used++] = '%'; out[used++] = hex[character >> 4]; out[used++] = hex[character & 15];
    }
  }
  out[used] = '\0';
  return 0;
}

static int save_content(const char *path, const char *content)
{
  char temporary_path[2100];
  int fd;
  int failed = 0;
  FILE *file;
  if (snprintf(temporary_path, sizeof(temporary_path), "%s.tmp.XXXXXX", path) >=
      (int)sizeof(temporary_path)) return -1;
  fd = mkstemp(temporary_path);
  if (fd < 0) return -1;
  file = fdopen(fd, "w");
  if (file == NULL) { close(fd); unlink(temporary_path); return -1; }
  if (fputs(content, file) == EOF) failed = 1;
  if (!failed && fflush(file) != 0) failed = 1;
  if (fclose(file) != 0) failed = 1;
  if (failed) {
    unlink(temporary_path);
    return -1;
  }
  if (rename(temporary_path, path) != 0) { unlink(temporary_path); return -1; }
  return 0;
}

int html_resource(http_request req, char *page)
{
  char encoded_path[3072];
  char path[1024];
  char safe_path[2048];
  char content[2048];
  char escaped_path[6144];
  char escaped_content[10240];
  char query_buf[64];
  char *query;
  FILE *file;
  struct stat file_stat;
  size_t read_count;

  snprintf(encoded_path, sizeof(encoded_path), "%s", req.target);
  query = strchr(encoded_path, '?');
  if (query != NULL) {
    *query++ = '\0';
    snprintf(query_buf, sizeof(query_buf), "%s", query);
  } else query_buf[0] = '\0';
  if (decode_url(encoded_path, path, sizeof(path), 0) < 0 ||
      url_encode_path(path, encoded_path, sizeof(encoded_path)) < 0 ||
      html_escape(path, escaped_path, sizeof(escaped_path)) < 0) return 0;
  if (build_safe_path(path, safe_path, sizeof(safe_path)) < 0 ||
      stat(safe_path, &file_stat) != 0) {
    page[0] = '\0';
    return 0;
  }
  if (S_ISDIR(file_stat.st_mode)) {
    snprintf(req.target, sizeof(req.target), "%s", path);
    return html_menu(req, page);
  }
  if (!S_ISREG(file_stat.st_mode)) {
    page[0] = '\0';
    return 0;
  }

  if (strcmp(req.method, "POST") == 0) {
    if (!action_is_edit(query_buf) || strncmp(req.body, "content=", 8) != 0 ||
        decode_url(req.body + 8, content, MAX_EDIT_CONTENT + 1, 1) < 0)
      return 0;
    if (save_content(safe_path, content) < 0) return 0;
    snprintf(page, 1024 * 1024,
             "<html><body><p>Saved <a href='%s'>%s</a>.</p></body></html>\n",
             encoded_path, escaped_path);
    return 1;
  }

  file = fopen(safe_path, "r");
  if (file == NULL) return 0;
  read_count = fread(content, 1, sizeof(content) - 1, file);
  fclose(file);
  content[read_count] = '\0';
  if (html_escape(content, escaped_content, sizeof(escaped_content)) < 0) return 0;
  if (action_is_edit(query_buf) && file_stat.st_size <= MAX_EDIT_CONTENT) {
    snprintf(page, 1024 * 1024,
             "<html><body><h2>Edit %s</h2><form method='post' action='%s?action=edit'>"
             "<textarea name='content' rows='24' cols='80'>%s</textarea>"
             "<br><input type='submit' value='Save'></form></body></html>\n",
             escaped_path, encoded_path, escaped_content);
  } else {
    if (file_stat.st_size <= MAX_EDIT_CONTENT)
      snprintf(page, 1024 * 1024,
               "<html><body><h2>%s</h2><p><a href='%s?action=edit'>Edit</a></p>"
               "<pre>%s</pre></body></html>\n",
               escaped_path, encoded_path, escaped_content);
    else
      snprintf(page, 1024 * 1024,
               "<html><body><h2>%s</h2><p>File is too large to edit.</p>"
               "<pre>%s</pre></body></html>\n", escaped_path, escaped_content);
  }
  return 1;
}
