#ifndef __POSTURE_CHECK_H__
#define __POSTURE_CHECK_H__

#ifdef __cplusplus
extern "C" {
#endif

void start_posture_recognize();
void stop_posture_recognize();

BOOL init_posture_model();
BOOL deinit_posture_model();


#ifdef __cplusplus
}
#endif //__cplusplus

#endif // !__POSTURE_CHECK_H__