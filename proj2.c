//#Author: Vojtech Sima (xsimav01)
//#Project: VUT FIT IOS Proj2
//#Date: 06.05.2020, Brno, Czechia
//#Inspirated The Little Book Of Smeaphores (author Allen B. Downey), chapter Faneuil Hall problem (page 219)
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>


#include <unistd.h>
#include <sys/types.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define MMAP(pointer) {(pointer) = mmap(NULL, sizeof(*(pointer)), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);}
#define UNMAP(pointer) {munmap((pointer), sizeof((pointer)));}
#define sleep_time(max_delay) {if (max_delay !=0 ) usleep((rand()%(max_delay+1))*1000);} //fce na random vyber casu mezi 0 a max_delay

//##### inicalizace proměnych
int PI; //pocet procesu (imigrantu)
int IG; // doba, po ktere je generovan novy imigrant
int JG; //doba po ktere soudce vstoupi opet do budovy
int IT; //doby trvani vyzvedavani certifikatu
int JT; //trvani rozhodnuti soudcem
int PI_2; //kopie poctu imigrantu (pro soudce)

//inicailizace semaforu (jako globalni)
sem_t *noJudge = NULL;
sem_t *writing = NULL; //semafor pro zapis (defualtne 1, vzdy se lockne, provede zapis a pak se zase odemkne)
sem_t *mutex = NULL; //blokuje to, aby mohli byt provadeny dalsi proces (kdyt judge in, nikdo nemuze dovnitr ani ven)
sem_t *allSignedIn=NULL; //kdyz je splneno, muze se zacit potvrzovat (vsichni imigranti jsou checknuty)
sem_t *confirmed=NULL;

int *NE = NULL;  //pocet imigrantu, kteri vstoupili (enter) do budovy ale nebylo rozhodnuto
int *NC = NULL;  //pocet imigrantu, kteri se zaregistrovali ale nebylo rozhodnuto
int *NB = NULL;  //pocet imigrantu aktualne v budově
int *counter = NULL; //counter jednotlivych kroku pro vypis
int *pom=0; //pomocna promena pro soudce 
FILE *file = NULL; //soubor pro vypis 


int kontrola_argumentu(int argc, char *arg, int i)
{	int kontrola=0; 
	if (argc != 6) //kontrola zda jsou zadany vsechny argumenty
	{
		fprintf(stderr, "Spatny pocet vstupnich argumentu\n");
		return 1;
	}

	for(unsigned int j=0; j<strlen(arg); j++) //kontrola že jsou zadany čisla
	{
		if(!(arg[j]>='0' && arg[j]<='9'))
		{	fprintf(stderr, "Spatne hodnoty vstupnich argumentu(mouhou byt pouze 0-9)\n");
			return 1;	
		}

	}
	kontrola=atoi(arg); //ulozeni hodnoty pro kontrolu delaye
	if (kontrola<0 || kontrola>2000) //delay 0-2000
		{
		fprintf(stderr, "Spatne hodnoty vstupnich argumentu\n");
		return 1;		
		}
	if(i==1)
	{if(kontrola<1) //minimalne musi byt zadan jeden imigrant
		{
		fprintf(stderr, "Spatny pocet imigrantu\n");
		return 1;
		}
	}
return 0;
}

int initalization()
{	
	file = fopen("proj2.out","w"); 
    if (file == NULL)
		{
			fprintf(stderr, "ERROR-Problem with output file\n"); //kdyz se nepodari otevrit soubor pro psani
			return -1;
		}
	setbuf(file,NULL); //aby se nebuferovalo a byl spravny vypis
	MMAP(counter);
    *counter = 1; //nastaveni counteru na 1(cislujeme od 1, vpodstate counter radku a provedenych procesu)
	MMAP(NE); //namapovani 
    MMAP(NC);
    MMAP(NB);
	MMAP(pom);

	MMAP(noJudge);
	MMAP(writing);
	MMAP(mutex);
	MMAP(allSignedIn);
	MMAP(confirmed);

	//int sem_init(sem_t *sem, int pshared, unsigned int value) --pointer na semafor, pshaerd 1 = sdilene pro vsechny, 0 lokalni, value vychozi hodnota
	//allSignedIn a confirmed maji defaultni hodnotu semaforu na 0 (nemuze byt wait hned, nejprve musi post dovolit)
	//sem_init() returns 0 on succes; on error, -1 is returned
	
    if ( ((noJudge=sem_open("/xsimav01.ios.proj2.noJudge", O_CREAT | O_EXCL, 0666, 1))==SEM_FAILED ) || 
		 ((writing=sem_open("/xsimav01.ios.proj2.writing", O_CREAT | O_EXCL, 0666, 1))==SEM_FAILED ) ||
		 ((mutex=sem_open("/xsimav01.ios.proj2.mutex", O_CREAT | O_EXCL, 0666, 1))==SEM_FAILED ) ||
		 ((allSignedIn=sem_open("/xsimav01.ios.proj2.allSignedIn", O_CREAT | O_EXCL, 0666, 0))==SEM_FAILED ) ||
		 ((confirmed=sem_open("/xsimav01.ios.proj2.confirmed", O_CREAT | O_EXCL, 0666, 0))==SEM_FAILED ))

		{	
			fprintf(stderr, "ERROR-PROBLEEM pri inicializaci semaforu\n");								
			return -1;
		}
    



	return 0;
}

void clean()
{
	UNMAP(counter);
	UNMAP(NE);
	UNMAP(NB);
	UNMAP(NE);

	if(file != NULL)
	{
		fclose(file);
	}

	sem_close(noJudge);
	sem_close(writing);
	sem_close(mutex);
	sem_close(allSignedIn);
	sem_close(confirmed);

	sem_unlink("/xsimav01.ios.proj2.noJudge");
	sem_unlink("/xsimav01.ios.proj2.writing");
	sem_unlink("/xsimav01.ios.proj2.mutex");
	sem_unlink("/xsimav01.ios.proj2.allSignedIn");
	sem_unlink("/xsimav01.ios.proj2.confirmed");

	UNMAP(noJudge);
	UNMAP(writing);
	UNMAP(mutex);
	UNMAP(allSignedIn);
	UNMAP(confirmed);
	UNMAP(pom);
}

void immigrant(int id)
{
	 sem_wait(writing); //starts(vytvoreni imigranta), muze se stat kdykoliv
        fprintf(file,"%d\t: IMM %d\t: starts\n", *counter, id);  
        (*counter)++; 
     sem_post(writing);

	 sem_wait(noJudge); //muze vstoupit poze pokud neni pritomen soudce
		sem_wait(writing); 
				(*NE)++; //vstoupil a nebylo orzhodnuto
				(*NB)++; //imigrantu v budove
				fprintf(file,"%d\t: IMM %d\t: enters\t\t: %d\t: %d\t: %d\n",*counter,id,*NE,*NC,*NB);			
				(*counter)++; 
		sem_post(writing);
	 sem_post(noJudge);

	 sem_wait(mutex);
		sem_wait(writing); 
			(*NC)++; //checknuti k rozhodnuti()
			fprintf(file,"%d\t: IMM %d\t: checks\t\t: %d\t: %d\t: %d\n",*counter,id,*NE,*NC,*NB);	
			(*counter)++; 
		sem_post(writing);
		
		if((*NC==*NE)&&(*pom==1)) //pokud vsichni zaregistrovani jsou checknuti 
		{	
		 sem_post(allSignedIn);
		}
		else
		{
		sem_post(mutex);
		}

	 

///-----------
	
	sem_wait(confirmed); //kdyzý soudce vyda rozhodnuti rozhodnuti
			sem_wait(writing); 
				fprintf(file,"%d\t: IMM %d\t: wants certificate\t: %d\t: %d\t: %d\n",*counter,id,*NE,*NC,*NB);				
				(*counter)++; 
			sem_post(writing);

			sleep_time(IT);

			sem_wait(writing); 
				fprintf(file,"%d\t: IMM %d\t: got certificate\t: %d\t: %d\t: %d\n",*counter,id,*NE,*NC,*NB);	
				(*counter)++; 
			sem_post(writing);
	 	

		sem_wait(noJudge); //leavnout lze pouze pokud neni soudce v budove (muze se vstupovat nebo leavovat)
			(*NB)--;
			sem_wait(writing); 
				fprintf(file,"%d\t: IMM %d\t: leaves\t\t: %d\t: %d\t: %d\n",*counter,id,*NE,*NC,*NB);
				(*counter)++; 
			sem_post(writing);



		sem_post(noJudge); 

exit(0);


}
void judge()
{	
	 while(PI_2>0) //dokud nebylo provodeno stejne rozhodnuti jako je pocet imigrantu tak se provadi
	 {
	sleep_time(JG);

	 sem_wait(writing); //starts(vytvoreni soudce), muze se stat kdykoliv	 	
		fprintf(file,"%d\t: JUDGE\t: wants to enter\n", *counter);
		(*counter)++; 		
	sem_post(writing);
	
	
	sem_wait(noJudge); //soudce vstoupi do budovy
		*pom=1;	
		sem_wait(writing);
			fprintf(file,"%d\t: JUDGE\t: enters\t\t: %d\t: %d\t: %d\n",*counter,*NE,*NC,*NB);
			(*counter)++; 
		sem_post(writing);

		
            if (*NE > *NC)
            {
                sem_wait(writing);
					fprintf(file,"%d\t: JUDGE\t: waits for imm\t: %d\t: %d\t: %d\n", *counter, *NE, *NC, *NB);
					(*counter)++;
                sem_post(writing);
				sem_post(mutex);
				sem_wait(allSignedIn);
				        
            }
            

				sem_wait(writing);
					fprintf(file,"%d\t: JUDGE\t: starts confirmation\t: %d\t: %d\t: %d\n",*counter,*NE,*NC,*NB);
					
					(*counter)++; 
				sem_post(writing);
			

		sleep_time(JT);

				sem_wait(writing);
					PI_2=PI_2-(*NC);
					for (int j = 0; j < *NC; j++) //musi se nascitat semafor confirmed aby mohli pak postupne opoustet imignratnti
						{
							sem_post(confirmed);
						}
					(*NC)=0;
					(*NE)=0;
					fprintf(file,"%d\t: JUDGE\t: ends confirmation\t: %d\t: %d\t: %d\n",*counter,*NE,*NC,*NB);
					(*counter)++;			
				sem_post(writing);
			
		sleep_time(JT);

		sem_wait(writing);
					
					fprintf(file,"%d\t: JUDGE\t: leaves\t\t: %d\t: %d\t: %d\n",*counter,*NE,*NC,*NB);
					(*counter)++;
					*pom=0;	 

		sem_post(writing);

	sem_post(mutex);	
	sem_post(noJudge); //vystup z budovy (mohou vstupovat imigranti nebo opet soudce)
	}
	sem_wait(writing);
					
					fprintf(file,"%d\t: JUDGE\t: finishes\n",*counter);
					(*counter)++; 

	sem_post(writing);
	exit(0);
}


void generator_imigrantu() //generovani daneho počtu imigrantu
{
	for (int i = 1; i <= PI; i++)
		{
			pid_t imigrant_proces=fork();
		
			if(imigrant_proces == 0)
			{
				immigrant(i);
			}
			
			sleep_time(IG);
		}
		 for (int i = 1; i <=PI; i++)
    	{
        wait(NULL);
    	}
	


	exit(0);
}



int main(int argc, char *argv[])
{   
	char *arg;	
	
	for(int i=1; i<6; i++)
	{
		arg=argv[i];
		if(kontrola_argumentu(argc,arg,i)==1)
		{ exit(1);}
	}
	PI=atoi(argv[1]); //ulozeni hodnot z argumentu
	PI_2=atoi(argv[1]);
	IG=atoi(argv[2]);
	JG=atoi(argv[3]);
	IT=atoi(argv[4]);
	JT=atoi(argv[5]);

  	
	if(initalization()!=0) //pokud neco spatne v inicaliziaci
	{	
		clean(); 
		exit(1);
	}

	pid_t imigranti_pid=fork();

    if (imigranti_pid == 0) //child procces
    {
        generator_imigrantu();
    }
	else if (imigranti_pid == -1) //když selže fork
	{
	fprintf(stderr, "Fork error");
	clean();
	exit(1);
	}
	//else {TODO ERROR}

	pid_t judge_pid=fork();
	 if (judge_pid == 0) //child procces
    {
       judge();
    }
	else if (judge_pid == -1)
	{
	fprintf(stderr, "Fork error");
	clean();
	exit(1);
	}

   
   
waitpid(imigranti_pid, NULL, 0);    //cekani na ukonceni procesu
waitpid(judge_pid, NULL, 0);
clean();
exit(0);
}




