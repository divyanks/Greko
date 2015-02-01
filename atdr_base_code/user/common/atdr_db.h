char db_user[10];
char db_pswrd[10];
char db_host[10];

struct variables
{
	char key[10];
	char *var;
};

struct variables Arr[] = {
	"username", db_user,
	"password", db_pswrd,
	"host", db_host
};

