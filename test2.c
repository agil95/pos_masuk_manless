#include <stdio.h> 
#include <cjson/cJSON.h> 

int main() { 
	// open the file 
	FILE *fp = fopen("test.json", "r"); 
	if (fp == NULL) { 
		printf("Error: Unable to open the file.\n"); 
		return 1; 
	} 

	// read the file contents into a string 
	char buffer[1024]; 
	int len = fread(buffer, 1, sizeof(buffer), fp); 
	fclose(fp); 

	// parse the JSON data 
	cJSON *json = cJSON_Parse(buffer); 
	if (json == NULL) { 
		const char *error_ptr = cJSON_GetErrorPtr(); 
		if (error_ptr != NULL) { 
			printf("Error: %s\n", error_ptr); 
		} 
		cJSON_Delete(json); 
		return 1; 
	}

    cJSON* namesub = NULL;
    cJSON* index = NULL;
    cJSON* optional = NULL;

    int i;

	// access the JSON data 
	cJSON *name = cJSON_GetObjectItemCaseSensitive(json, "status"); 
	if (cJSON_IsString(name) && (name->valuestring != NULL)) { 
		printf("Status: %s\n", name->valuestring); 
	} else {
        printf("Status: %s\n", name->valuestring); 
    }

    cJSON *data = cJSON_GetObjectItem(json, "data");
    for (i = 0; i < cJSON_GetArraySize(data); i++)
    {
        cJSON * subitem = cJSON_GetArrayItem(data, i);
        namesub = cJSON_GetObjectItem(subitem, "name");
        index = cJSON_GetObjectItem(subitem, "index");
        optional = cJSON_GetObjectItem(subitem, "optional"); 
        printf("Name[%d]: %s\n", i, namesub->valuestring);
        printf("Optional[%d]: %d\n", i, optional->valueint);
    }

	// delete the JSON object 
	cJSON_Delete(json); 
	return 0; 
}
