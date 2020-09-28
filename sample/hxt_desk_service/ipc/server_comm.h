#ifndef __server_comm_h__
#define __server_comm_h__

#ifdef __cplusplus
extern "C" {
#endif

int connect_to_mpp_service();
int send_setup_video_ratio_cmd(int width, int height);
int send_posture_start_cmd(int width, int height);
int send_posture_stop_cmd();
int send_voice_sample_start_cmd();
int send_voice_sample_stop_cmd();
int send_recording_mp4_cmd(const char* video_file, const char* snap_file);
int send_delete_mp4_cmd();
int send_stop_record_mp4_cmd();




#ifdef __cplusplus
}
#endif //__cplusplus

#endif // !__server_comm_h__