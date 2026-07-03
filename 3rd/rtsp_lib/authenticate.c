#include <stdio.h>
#include "our_md5.h"
#include "authenticate.h"
//#include "../utils/CLog.h"

int GetCResponse(char *pInputBuf,char *pCResponse)	//获取客户端的Response
{
	char* pRet=strstr(pInputBuf,"response=\"");
	if(NULL==pRet){
        //LOGW("Can not find response in msg");
		fflush(stdout);
		return -1;
	}else{
		memset(pCResponse,0,sizeof(pCResponse));
		sscanf(pRet+10,"%[^\"]",pCResponse);
		return 0;
	}
}

int GetUri(char *pInputBuf,char *pUri)	//获取客户端的Response
{
	char* pRet=strstr(pInputBuf,"uri=\"");
	if(NULL==pRet){
        //LOGW("Can not find uri in msg");
		fflush(stdout);
		return -1;
	}else{
		memset(pUri,0,sizeof(pUri));
		sscanf(pRet+5,"%[^\"]",pUri);
		return 0;
	}
}

int GetCmdName(char *pInputBuf,char *pCmdName)	//获取客户端的Response
{
	memset(pCmdName,0,sizeof(pCmdName));
	sscanf(pInputBuf,"%[A-Z]",pCmdName);
	return 0;
}

char* GetSResponse(char* uri,char *pServerUsername,char *pServerPassword,char *pCmdName)	//获取服务器的Response
{
		// The "response" field is computed as:  
		//    md5(md5(<username>:<realm>:<password>):<nonce>:md5(<cmd>:<url>))  
		// or, if "fPasswordIsMD5" is True:  
		//    md5(<password>:<nonce>:md5(<cmd>:<url>))       
		//username="user1", realm="rtspserver", nonce="ab1234c446bfa49b0c48c3edb83139543d", uri="rtsp://192.168.18.36:8600/1", response="01d4967a0d9f2c3814bf2c2a6c106b48"
		//md5(A:B:C) 则A=md5格式password 或 md5(<username>:<realm>:<明文password>) B=nonce C=md5(<cmd>:<uri>)
//		char* realm="LIVE555 Streaming Media";
		char* realm="rtsp_demo";
		char* nonce="ab1234c446bfa49b0c48c3edb83139543d";
		//char* url="rtsp://192.168.18.36:8600/1";
//		char* cmd="DESCRIBE";
		char ha1Buf[33];  

		unsigned char fPasswordIsMD5=1;
		char* username=pServerUsername;
		char* password=pServerPassword;
		fPasswordIsMD5=0;
		if (fPasswordIsMD5) {  
				strncpy(ha1Buf, password, 32); //这里的password是密文 
				ha1Buf[32] = '\0'; // just in case  
		} else {  
				unsigned const ha1DataLen = strlen(username) + 1  
						+ strlen(realm) + 1 + strlen(password); //这里的password是明文 
				unsigned char* ha1Data = (unsigned char*)malloc(ha1DataLen+1);  
				sprintf((char*)ha1Data, "%s:%s:%s", username, realm, password);  
				our_MD5Data(ha1Data, ha1DataLen, ha1Buf);  
//				printf("ha1Data=%s,ha1Buf=%s\n",ha1Data,ha1Buf);
				free(ha1Data);  
		}  

//		memset(ha1Buf,'\0',33);
//		strncpy(ha1Buf,"ANH38GuhdeYqA",32);


		unsigned const ha2DataLen = strlen(pCmdName) + 1 + strlen(uri);  
		unsigned char* ha2Data = (unsigned char*)malloc(ha2DataLen+1);  
		sprintf((char*)ha2Data, "%s:%s", pCmdName, uri);  
		char ha2Buf[33];  
		our_MD5Data(ha2Data, ha2DataLen, ha2Buf);  
		free(ha2Data);  
		//delete[] ha2Data;  

		unsigned const digestDataLen  
				= 32 + 1 + strlen(nonce) + 1 + 32;  
		unsigned char* digestData = (unsigned char*)malloc(digestDataLen+1);  
		sprintf((char*)digestData, "%s:%s:%s",  
						ha1Buf, nonce, ha2Buf);  
		char const* result = our_MD5Data(digestData, digestDataLen, NULL);  
		free(digestData);  
		//delete[] digestData;  
		return (char*)result;  
}


int  RtspAuthenticateDigest(char *pInputBuf,char *pServerUsername,char *pServerPassword) //摘要认证
{
	char szCResponse[33];		//客户端的Response
	char szSResponse[33];		//服务端的Response
	char szUri[128];			//url
	char szCmdName[16];

	if((0==GetCResponse(pInputBuf,szCResponse))&&(0==GetUri(pInputBuf,szUri))&&(0==GetCmdName(pInputBuf,szCmdName))){
			sprintf(szSResponse,"%s",GetSResponse(szUri,pServerUsername,pServerPassword,szCmdName));
//			printf("===============RtspAuthenticateDigest,szSResponse=%s\n",szSResponse);fflush(stdout);
			if(memcmp(szSResponse,szCResponse,strlen(szSResponse))==0)
				return 0;

	}
	return -1;	
}



