#include <stdlib.h>
#include <stdio.h>
#include <jni.h>
#include <stdlib.h>
#include <android/log.h>

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT jint JNICALL Java_myjni_MyJNI_exec
  (JNIEnv *, jobject, jstring);

#ifdef __cplusplus
}
#endif

JNIEXPORT jint Java_myjni_MyJNI_exec
  (JNIEnv *env, jobject obj, jstring filename)
{
    char bufFilename[256] = {0};
    const char *strFilename;
	int ret = 0;

    strFilename = (*env)->GetStringUTFChars(env, filename, NULL);
    if (strFilename == NULL) {
        return -1; /* OutOfMemoryError already thrown */
    }
    //printf("%s", strFilename);
    snprintf(bufFilename, sizeof(bufFilename) - 1, "%s", strFilename);
    (*env)->ReleaseStringUTFChars(env, filename, strFilename);

	system(bufFilename);
	
	return ret;
}
