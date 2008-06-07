#include <stdio.h>
#include <curl/curl.h>

#include "breakin.h"

size_t handle_response(void *ptr, size_t size, size_t nmeb, void *stream) {
	if (strncmp(ptr, "OK", 2) == 0) {
		printf("Yea!\n");
	}
	else {
		printf("Bo! %s\n", ptr);
	}

	return (size * nmeb);
}

int upload_data(char *id, char *url, char *filename) {

	CURL *curl;
	CURLcode res;
	struct curl_httppost *formpost = NULL;
	struct curl_httppost *lastptr = NULL;
	char error_msg[CURL_ERROR_SIZE] = "";
	long code;
	char cur_time[BUFSIZ], buf[BUFSIZ];
	int run_time;

	run_time = time(NULL) - start_time;

	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "data",
		CURLFORM_FILE, filename, CURLFORM_END);

	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "id",
		CURLFORM_COPYCONTENTS, id, CURLFORM_END);

	sprintf(buf, "%d", run_time);
	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "time",
		CURLFORM_COPYCONTENTS, buf, CURLFORM_END);

	curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(curl, CURLOPT_URL, url);

	curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_msg);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, handle_response);

	res = curl_easy_perform(curl);

	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);

	curl_formfree(formpost);

	return 0;
}

int main(int argc, char **argv) {

	upload_data("123456", "http://localhost/cgi-bin/breakin_server.pl", 
		"/var/run/breakin.dat");

}
