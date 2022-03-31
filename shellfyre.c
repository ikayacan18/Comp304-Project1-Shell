// NAME: Ismail Ozan Kayacan
// ID:   69103

#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h> //termios, TCSANOW, ECHO, ICANON
#include <string.h>
#include <stdbool.h>
#include <errno.h>
const char *sysname = "shellfyre";

//for cdh
#include<ctype.h> //for isspace() function.
const char *letters = "abcdefghjk";
static FILE *fp=NULL;
static char *txt_path;
static char cwd[100];

#include<sys/stat.h> //for mkdir() function.
#include<sys/types.h> //for mkdir() modes.

#include<dirent.h> //for getting the files in directory.
#include<fcntl.h> //for open() function.

//for multexercise command.
#include<sys/wait.h> //for informing parent when child ends.
#include<poll.h> //not to wait for getting input forever.

enum return_codes
{
	SUCCESS = 0,
	EXIT = 1,
	UNKNOWN = 2,
};

struct command_t
{
	char *name;
	bool background;
	bool auto_complete;
	int arg_count;
	char **args;
	char *redirects[3];		// in/out redirection
	struct command_t *next; // for piping
};

/**
 * Prints a command struct
 * @param struct command_t *
 */
void print_command(struct command_t *command)
{
	int i = 0;
	printf("Command: <%s>\n", command->name);
	printf("\tIs Background: %s\n", command->background ? "yes" : "no");
	printf("\tNeeds Auto-complete: %s\n", command->auto_complete ? "yes" : "no");
	printf("\tRedirects:\n");
	for (i = 0; i < 3; i++)
		printf("\t\t%d: %s\n", i, command->redirects[i] ? command->redirects[i] : "N/A");
	printf("\tArguments (%d):\n", command->arg_count);
	for (i = 0; i < command->arg_count; ++i)
		printf("\t\tArg %d: %s\n", i, command->args[i]);
	if (command->next)
	{
		printf("\tPiped to:\n");
		print_command(command->next);
	}
}

/**
 * Release allocated memory of a command
 * @param  command [description]
 * @return         [description]
 */
int free_command(struct command_t *command)
{
	if (command->arg_count)
	{
		for (int i = 0; i < command->arg_count; ++i)
			free(command->args[i]);
		free(command->args);
	}
	for (int i = 0; i < 3; ++i)
		if (command->redirects[i])
			free(command->redirects[i]);
	if (command->next)
	{
		free_command(command->next);
		command->next = NULL;
	}
	free(command->name);
	free(command);
	return 0;
}

/**
 * Show the command prompt
 * @return [description]
 */
int show_prompt()
{
	char cwd[1024], hostname[1024];
	gethostname(hostname, sizeof(hostname));
	getcwd(cwd, sizeof(cwd));
	printf("%s@%s:%s %s$ ", getenv("USER"), hostname, cwd, sysname);
	return 0;
}

/**
 * Parse a command string into a command struct
 * @param  buf     [description]
 * @param  command [description]
 * @return         0
 */
int parse_command(char *buf, struct command_t *command)
{
	const char *splitters = " \t"; // split at whitespace
	int index, len;
	len = strlen(buf);
	while (len > 0 && strchr(splitters, buf[0]) != NULL) // trim left whitespace
	{
		buf++;
		len--;
	}
	while (len > 0 && strchr(splitters, buf[len - 1]) != NULL)
		buf[--len] = 0; // trim right whitespace

	if (len > 0 && buf[len - 1] == '?') // auto-complete
		command->auto_complete = true;
	if (len > 0 && buf[len - 1] == '&') // background
		command->background = true;

	char *pch = strtok(buf, splitters);
	command->name = (char *)malloc(strlen(pch) + 1);
	if (pch == NULL)
		command->name[0] = 0;
	else
		strcpy(command->name, pch);

	command->args = (char **)malloc(sizeof(char *));

	int redirect_index;
	int arg_index = 0;
	char temp_buf[1024], *arg;

	while (1)
	{
		// tokenize input on splitters
		pch = strtok(NULL, splitters);
		if (!pch)
			break;
		arg = temp_buf;
		strcpy(arg, pch);
		len = strlen(arg);

		if (len == 0)
			continue;										 // empty arg, go for next
		while (len > 0 && strchr(splitters, arg[0]) != NULL) // trim left whitespace
		{
			arg++;
			len--;
		}
		while (len > 0 && strchr(splitters, arg[len - 1]) != NULL)
			arg[--len] = 0; // trim right whitespace
		if (len == 0)
			continue; // empty arg, go for next

		// piping to another command
		if (strcmp(arg, "|") == 0)
		{
			struct command_t *c = malloc(sizeof(struct command_t));
			int l = strlen(pch);
			pch[l] = splitters[0]; // restore strtok termination
			index = 1;
			while (pch[index] == ' ' || pch[index] == '\t')
				index++; // skip whitespaces

			parse_command(pch + index, c);
			pch[l] = 0; // put back strtok termination
			command->next = c;
			continue;
		}

		// background process
		if (strcmp(arg, "&") == 0)
			continue; // handled before

		// handle input redirection
		redirect_index = -1;
		if (arg[0] == '<')
			redirect_index = 0;
		if (arg[0] == '>')
		{
			if (len > 1 && arg[1] == '>')
			{
				redirect_index = 2;
				arg++;
				len--;
			}
			else
				redirect_index = 1;
		}
		if (redirect_index != -1)
		{
			command->redirects[redirect_index] = malloc(len);
			strcpy(command->redirects[redirect_index], arg + 1);
			continue;
		}

		// normal arguments
		if (len > 2 && ((arg[0] == '"' && arg[len - 1] == '"') || (arg[0] == '\'' && arg[len - 1] == '\''))) // quote wrapped arg
		{
			arg[--len] = 0;
			arg++;
		}
		command->args = (char **)realloc(command->args, sizeof(char *) * (arg_index + 1));
		command->args[arg_index] = (char *)malloc(len + 1);
		strcpy(command->args[arg_index++], arg);
	}
	command->arg_count = arg_index;
	return 0;
}

void prompt_backspace()
{
	putchar(8);	  // go back 1
	putchar(' '); // write empty over
	putchar(8);	  // go back 1 again
}

/**
 * Prompt a command from the user
 * @param  buf      [description]
 * @param  buf_size [description]
 * @return          [description]
 */
int prompt(struct command_t *command)
{
	int index = 0;
	char c;
	char buf[4096];
	static char oldbuf[4096];

	// tcgetattr gets the parameters of the current terminal
	// STDIN_FILENO will tell tcgetattr that it should write the settings
	// of stdin to oldt
	static struct termios backup_termios, new_termios;
	tcgetattr(STDIN_FILENO, &backup_termios);
	new_termios = backup_termios;
	// ICANON normally takes care that one line at a time will be processed
	// that means it will return if it sees a "\n" or an EOF or an EOL
	new_termios.c_lflag &= ~(ICANON | ECHO); // Also disable automatic echo. We manually echo each char.
	// Those new settings will be set to STDIN
	// TCSANOW tells tcsetattr to change attributes immediately.
	tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);

	// FIXME: backspace is applied before printing chars
	show_prompt();
	int multicode_state = 0;
	buf[0] = 0;

	while (1)
	{
		c = getchar();
		// printf("Keycode: %u\n", c); // DEBUG: uncomment for debugging

		if (c == 9) // handle tab
		{
			buf[index++] = '?'; // autocomplete
			break;
		}

		if (c == 127) // handle backspace
		{
			if (index > 0)
			{
				prompt_backspace();
				index--;
			}
			continue;
		}
		if (c == 27 && multicode_state == 0) // handle multi-code keys
		{
			multicode_state = 1;
			continue;
		}
		if (c == 91 && multicode_state == 1)
		{
			multicode_state = 2;
			continue;
		}
		if (c == 65 && multicode_state == 2) // up arrow
		{
			int i;
			while (index > 0)
			{
				prompt_backspace();
				index--;
			}
			for (i = 0; oldbuf[i]; ++i)
			{
				putchar(oldbuf[i]);
				buf[i] = oldbuf[i];
			}
			index = i;
			continue;
		}
		else
			multicode_state = 0;

		putchar(c); // echo the character
		buf[index++] = c;
		if (index >= sizeof(buf) - 1)
			break;
		if (c == '\n') // enter key
			break;
		if (c == 4) // Ctrl+D
			return EXIT;
	}
	if (index > 0 && buf[index - 1] == '\n') // trim newline from the end
		index--;
	buf[index++] = 0; // null terminate string

	strcpy(oldbuf, buf);

	parse_command(buf, command);

	// print_command(command); // DEBUG: uncomment for debugging

	// restore the old settings
	tcsetattr(STDIN_FILENO, TCSANOW, &backup_termios);
	return SUCCESS;
}

int process_command(struct command_t *command);


int find_dirs(DIR *dir, char* to_search, bool open, bool rec){

	
	struct dirent *de;
	char founds[50][50];
	
	while((de=readdir(dir))!=NULL){
		
		if(strstr(de->d_name, to_search)!=NULL){
			//getcwd(cwd, sizeof(cwd));
			//strcat(cwd, "/");
			//strcat(cwd, de->d_name);
			
			/*for (int i=0; i<50; i++){
				if (strcmp(founds[i], de->d_name)==0) return 0;
			}
			strcpy(founds[found], de->d_name);
			found+=1;*/
			
			printf("%s\n", de->d_name);
			if(open) {
				
				pid_t pid = fork();
				if (pid == 0) // child
				{	
					execl("/bin/xdg-open",  "xdg-open", de->d_name, NULL);
					exit(0);
				} else {
					wait(NULL);
					if(rec){
						DIR *newdir;
						if((newdir=opendir(de->d_name))!=NULL){
							find_dirs(newdir, to_search, open, rec);
						} else {
							//return SUCCESS;
						}
					}
				}
				
			} else {
			
				if(rec){
					DIR *newdir;
					if((newdir=opendir(de->d_name))!=NULL){
						//chdir(de->d_name);
						find_dirs(newdir, to_search, open, rec);
					} else {
						//return SUCCESS;
					}
				}
			}
			
		} else {
			
			if(rec){
				
				DIR *newdir;
				if((newdir=opendir(de->d_name))!=NULL){
					find_dirs(newdir, to_search, open, rec);
				} else {
					//return SUCCESS;
				}
				
			}
		}
	}
	closedir(dir);
	return SUCCESS;
}


int main()
{
	// path for txt file which stores visited directories. (for cdh)
	txt_path=getenv("HOME");
	strcat(txt_path, "/old_dirs.txt");
	
	
	while (1)
	{
		struct command_t *command = malloc(sizeof(struct command_t));
		memset(command, 0, sizeof(struct command_t)); // set all bytes to 0

		int code;
		code = prompt(command);
		if (code == EXIT)
			break;

		code = process_command(command);
		if (code == EXIT)
			break;

		free_command(command);
	}

	printf("\n");
	return 0;
}

int process_command(struct command_t *command)
{
	int r;
	if (strcmp(command->name, "") == 0)
		return SUCCESS;

	if (strcmp(command->name, "exit") == 0)
		return EXIT;

	if (strcmp(command->name, "cd") == 0)
	{
		if (command->arg_count > 0)
		{
			
			
			r = chdir(command->args[0]);
			
			//record the new directory (for cdh command).
			getcwd(cwd, sizeof(cwd));
			fp=fopen(txt_path, "a");
			fprintf(fp, "%s\n", cwd);
			fclose(fp);
			
			if (r == -1)
				printf("-%s: %s: %s\n", sysname, command->name, strerror(errno));
			return SUCCESS;
		}
	}

	// TODO: Implement your custom commands here
	if (strcmp(command->name, "filesearch") == 0)
	{
		int arg_count=command->arg_count;
		bool rec=false;
		bool open=false;
		char *to_search=command->args[arg_count-1];
		
		if(arg_count==3){
			if(strcmp(command->args[0], "-r")==0 ||strcmp(command->args[1], "-r")==0) rec=true;
			if(strcmp(command->args[0], "-o")==0 ||strcmp(command->args[1], "-o")==0) open=true;
		} else if(arg_count==2){
			if(strcmp(command->args[0], "-r")==0) rec=true;
			if(strcmp(command->args[0], "-o")==0) open=true;
		}
		 
		struct dirent *de;
		DIR *dir = opendir(".");
		find_dirs(dir, to_search, open, rec);
		//getcwd(cwd, sizeof(cwd));
		/*while((de=readdir(dir))!=NULL){
			if(strstr(de->d_name, to_search)!=NULL){
				printf("%s\n", de->d_name);
				if(open) execl("/bin/xdg-open",  "xdg-open", de->d_name, NULL);
				//if(rec)
			}
		}*/
	}
	
	if (strcmp(command->name, "cdh") == 0)
	{
		if (command->arg_count == 0)
		{
			
			fp=fopen(txt_path, "r");
			
			char dirs[10][50];
			char line[50];
			
			// find the count of lines
			int count=0;
			while(fgets(line, sizeof(line), fp)){
				count+=1;	
			}
			
			fclose(fp);
			

			//if there are less than 10 old directory, directly use them.
			if(count<=10){
			
				//store the directories in dirs array.
				fp=fopen(txt_path, "r");
				int index=0;
				while(fgets(line, sizeof(line), fp)){
					strcpy(dirs[count], line);	
					index+=1;
				}
				fclose(fp);
				
				//print directories.
				for(int i=0; i<count; i++){
					printf("%c %d) %s", letters[count-i-1], count-i, dirs[i]);
				}
				
			//if there are more than 10 old directory, use last 10 of them.
			} else {
				int skip=count-10;
				
				//store the directories in dirs array.
				int index=0;
				fp=fopen(txt_path, "r");
				while(fgets(line, sizeof(line), fp)){
					if(skip>0){
						skip--;
					} else {
						strcpy(dirs[index], line);
						index+=1;
					}	
				}
				fclose(fp);
				
				//print directories.
				for(int i=0; i<10; i++){
					printf("%c %d) %s", letters[10-i-1], 10-i, dirs[i]);
				}
			}
			
			printf("Select directory by letter or number: ");
				
				// get the input from user
				char choice[3];
				fgets(choice, sizeof(choice), stdin);
				
				char path[50];
	
				// according to choice of user, get the directory to go.
				if(strstr(choice, "k")!=NULL || strstr(choice, "10")!=NULL) strcpy(path,dirs[0]);
				else if(strstr(choice, "a")!=NULL || strstr(choice, "1")!=NULL) strcpy(path,dirs[9]);
				else if(strstr(choice, "b")!=NULL || strstr(choice, "2")!=NULL) strcpy(path,dirs[8]);
				else if(strstr(choice, "c")!=NULL|| strstr(choice, "3")!=NULL) strcpy(path,dirs[7]);
				else if(strstr(choice, "d")!=NULL || strstr(choice, "4")!=NULL) strcpy(path,dirs[6]);
				else if(strstr(choice, "e")!=NULL || strstr(choice, "5")!=NULL) strcpy(path,dirs[5]);
				else if(strstr(choice, "f")!=NULL|| strstr(choice, "6")!=NULL) strcpy(path,dirs[4]);
				else if(strstr(choice, "g")!=NULL|| strstr(choice, "7")!=NULL) strcpy(path,dirs[3]);
				else if(strstr(choice, "h")!=NULL || strstr(choice, "8")!=NULL) strcpy(path,dirs[2]);
				else if(strstr(choice, "j")!=NULL|| strstr(choice, "9")!=NULL) strcpy(path,dirs[1]);
				
				// get rid of space at the end of the line.
				for(int i=0; i<strlen(path); i++){
					if(isspace(path[i])){
						path[i]='\0';
					}
				}
				
				// go new directory.
				chdir(path);
				
				//record the new directory (for cdh command).
				getcwd(cwd, sizeof(cwd));
				fp=fopen(txt_path, "a");
				fprintf(fp, "%s\n", cwd);
				fclose(fp);
			
			return SUCCESS;
		}
	}
	
	if (strcmp(command->name, "take") == 0)
	{
		char *dir;
		dir=strtok(command->args[0], "/");
		while(dir!=NULL){
			if (chdir(dir) != 0){
				mkdir(dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
				chdir(dir);
			}
			dir=strtok(NULL, "/");
		}
		return SUCCESS;
	}
	
	if (strcmp(command->name, "joker") == 0)
	{
		//TODO
		
	}
	
	if (strcmp(command->name, "multexercise") == 0)
	{
		// Hardness level might be added.
		
		int correct_answers=0;
		int wrong_answers=0;
		double score=0;
		char answer[20];
		int ans;
		int correct_answer;
		int first_num;
		int second_num;
		//int op_index;
		//char op;
		bool first_time=true;
		bool answer_found=false;
		
		int duration=20;
		if(command->arg_count > 0){
			duration=atoi(command->args[0]);
		}
		
		printf("\nYou have %d seconds, each correct answer = 1 point, each wrong answer = -0.5 point. Good luck!\n", duration);
		sleep(1);
		
		struct pollfd mypool={STDIN_FILENO, POLLIN|POLLPRI};
		
		pid_t pid = fork();
		
		int status;
		if (pid == 0) // child
		{
			sleep(duration);
			exit(1);
		}
		else
		 do {
			
			
			if((pid=waitpid(pid,&status,WNOHANG))==-1) perror("wait() error");
			else if (pid==0){
				
				if(first_time || answer_found){
					
					first_num=rand()%20;
					second_num=rand()%20;
					
					//commented below was to randomly ask addition, substraction as well as multiplication. but did not work properly.
					
					/*op_index=rand()%4;
		
					if(op_index==0) {
						op='+';
						correct_answer=first_num+second_num;
					} else if (op_index==1){
					  	op='-';
					  	correct_answer=first_num-second_num;
					} else if (op_index==2){
					  	op='*';p
					  	correct_answer=first_num*second_num;
					}*/
					
					correct_answer=first_num*second_num;
					
					first_time=false;
					answer_found=false;
					//printf("%d%c%d=?\n", first_num, op, second_num);
					printf("%dx%d=?\n", first_num, second_num);	
				}
				
				if(poll(&mypool, 1, 3000)){
					fgets(answer, sizeof(answer), stdin);
					ans=atoi(answer);
					if(ans==correct_answer){
						printf("Correct!\n");
						answer_found=true;
						correct_answers+=1;
					} else {
						printf("Wrong!\n");
						wrong_answers+=1;	
					}
				} else {
					printf("Time is flowing! You still did not answer. Write a number!\n");
				}
					
			} else {
				if(WIFEXITED(status)) {
					score=correct_answers-0.5*wrong_answers;
					printf("\nTime is over! Your results:\n\nCorrect answers: %d\nWrong answers: %d\nScore:%f\n\n", correct_answers, wrong_answers, score);
					return SUCCESS;
				}
				else puts("child did not exit succesfully.");
			}
			
			
			
		 } while(pid==0);
		return SUCCESS;
	}
	
	pid_t pid = fork();

	if (pid == 0) // child
	{
		// increase args size by 2
		command->args = (char **)realloc(
			command->args, sizeof(char *) * (command->arg_count += 2));

		// shift everything forward by 1
		for (int i = command->arg_count - 2; i > 0; --i)
			command->args[i] = command->args[i - 1];

		// set args[0] as a copy of name
		command->args[0] = strdup(command->name);
		// set args[arg_count-1] (last) to NULL
		command->args[command->arg_count - 1] = NULL;

		/// TODO: do your own exec with path resolving using execv()
		
		// PART-1
		char *newargv[command->arg_count];
		char path[30]="/bin/";
		strcat(path, command->args[0]);
		execv(path, command->args);
		
		exit(0);
	}
	else
	{
		/// TODO: Wait for child to finish if command is not running in background
		
		// PART-1 CONTINUED
		if(!command->background){
			wait(NULL);
		}
		
		return SUCCESS;
	}

	printf("-%s: %s: command not found\n", sysname, command->name);
	return UNKNOWN;
}
