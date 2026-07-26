#ifndef __LOLA_LOG_H__
#define __LOLA_LOG_H__

#ifdef __linux__
  #define LOGE(...) fprintf(stderr, __VA_ARGS__)
  #define LOGI(...) fprintf(stdout, __VA_ARGS__)
  #define LOGD(...) fprintf(stdout, __VA_ARGS__)
  #define HERE LOGD("HERE: %s:%d\n", __FILE__, __LINE__)
  #define IN fprintf(stdout, ">> [%s], %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
  #define OUT fprintf(stdout, "<< [%s] %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif // __linux__

#endif // __LOLA_LOG_H__