#include <alsa/asoundlib.h>
#include <curses.h>
#include <dirent.h>
#include <stdio.h>

#include "const.h"

void open_music_file(const char *path_name) {
    fp = fopen(path_name, "rb");

    if (fp == NULL) {
        printf("Music File is NULL\n");
        fclose(fp);
        exit(1);
    }

    fseek(fp, 0, SEEK_SET);

    wav_header_size = fread(&wav_header, 1, sizeof(wav_header), fp);
}

bool debug_msg(int result, const char *str) {
    if (result < 0) {
        printf("err: %s 失败!, result = %d, err_info = %s \n", str, result, snd_strerror(result));
        exit(1);
    }
    return true;
}

void setup_snd_pcm(const char *path_name) {
    int is_little_endian = 0;
    if (strncmp(wav_header.chunk_id, "RIFF", 4) == 0) {
        is_little_endian = 1;
    }

    debug_msg(snd_pcm_hw_params_malloc(&hw_params), "分配snd_pcm_hw_params_t结构体");

    pcm_name = strdup("default");

    // debug_msg(snd_pcm_open(&pcm_handle, pcm_name, stream, 0), "打开PCM设备");
    while (snd_pcm_open(&pcm_handle, pcm_name, stream, 0) != 0)
        ;
    debug_msg(snd_pcm_hw_params_any(pcm_handle, hw_params), "配置空间初始化");
    debug_msg(snd_pcm_hw_params_set_access(pcm_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED), "设置交错模式（访问模式）");
    debug_msg(snd_pcm_hw_params_set_rate(pcm_handle, hw_params, wav_header.sample_rate * speed, 0), "设置采样率");
    debug_msg(snd_pcm_hw_params_set_channels(pcm_handle, hw_params, wav_header.num_channels), "设置通道数");
    buffer_size = period_size * periods;

    if (wav_header.bits_per_sample == 16) {
        frames = buffer_size / (16 * wav_header.num_channels / 8);

        debug_msg(snd_pcm_hw_params_set_buffer_size(pcm_handle, hw_params, frames), "设置S16_LE OR S16_BE缓冲区");
        if (is_little_endian) {
            pcm_format = SND_PCM_FORMAT_S16_LE;
        } else {
            pcm_format = SND_PCM_FORMAT_S16_BE;
        }
    } else if (wav_header.bits_per_sample == 24) {
        frames = buffer_size / (24 * wav_header.num_channels / 8);

        /*
                当位数为24时，就需要除以6了，因为是24bit * 2 / 8 = 6
        */
        debug_msg(snd_pcm_hw_params_set_buffer_size(pcm_handle, hw_params, frames), "设置S24_3LE OR S24_3BE的缓冲区");
        if (is_little_endian) {
            pcm_format = SND_PCM_FORMAT_S24_LE;
        } else {
            pcm_format = SND_PCM_FORMAT_S24_BE;
        }
    } else if (wav_header.bits_per_sample == 32) {
        frames = buffer_size / (32 * wav_header.num_channels / 8);
        /*
                当位数为32时，就需要除以8了，因为是32bit * 2 / 8 = 8
        */
        debug_msg(snd_pcm_hw_params_set_buffer_size(pcm_handle, hw_params, frames), "设置S32_LE OR S32_BE OR S24_LE OR S24_BE缓冲区");
        if (is_little_endian) {
            pcm_format = SND_PCM_FORMAT_S32_LE;
        } else {
            pcm_format = SND_PCM_FORMAT_S32_BE;
        }
    }
    debug_msg(snd_pcm_hw_params_set_format(pcm_handle, hw_params, pcm_format), "设置样本长度(位数)");
    debug_msg(snd_pcm_hw_params(pcm_handle, hw_params), "设置的硬件配置参数");
}

int main() {
    char *music_names[100];
    int music_count = 0, current_music = 0;

    DIR *d;
    struct dirent *dir;
    chdir("./music");
    d = opendir(".");

    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, "..")) {
            } else {
                char *path_name = (char *)malloc(512);
                strncpy(path_name, dir->d_name, 512);
                music_names[music_count] = path_name;
                music_count++;
            }
        }
        if (music_count == 0) {
            exit(1);
        }
    } else {
        exit(1);
    }

    // setup snd pcm
    open_music_file(music_names[current_music]);
    setup_snd_pcm(music_names[current_music]);
    buff = (unsigned char *)malloc(buffer_size);
    // setup window
    WINDOW *main_win = initscr();

    if (main_win == NULL) {
        exit(EXIT_FAILURE);
    }
    nodelay(main_win, 1);

    cbreak();
    noecho();

    int height, width, start_y, start_x;
    height = 40;
    width = 60;
    start_y = start_x = 0;

    WINDOW *win = newwin(height, width, start_y, start_x);
    refresh();

    int ret = 0;

    mvwprintw(win, 1, 1, "Currently Playing: %s", music_names[0]);
    mvwprintw(win, 2, 1, "Current Volume: %f", (volume + 1));
    mvwprintw(win, 3, 1, "Current Speed: %f", (speed));
    wrefresh(win);

    // feof函数检测文件结束符，结束：非0, 没结束：0 !feof(fp)
    while (1) {
        // read input from user
        int c = getch();
        if (c > 0) {
            if (c == 'h') {
                fseek(fp, -10 * buffer_size, SEEK_CUR);
            } else if (c == 'l') {
                fseek(fp, 10 * buffer_size, SEEK_CUR);
            }
            if (c == 'j') {
                volume += VOLUME_STEP;
                if (volume < -1) {
                    volume = -1;
                } else if (volume > 1) {
                    volume = 1;
                }
            } else if (c == 'k') {
                volume -= VOLUME_STEP;
                if (volume < -1) {
                    volume = -1;
                } else if (volume > 1) {
                    volume = 1;
                }
            }
            if (c == 's') {
                speed += SPEED_STEP;
                if (speed < 0.25) {
                    speed = 0.25;
                } else if (speed > 2) {
                    speed = 2;
                }
                snd_pcm_close(pcm_handle);
                snd_pcm_hw_params_free(hw_params);

                setup_snd_pcm(music_names[current_music]);
            } else if (c == 'a') {
                speed -= SPEED_STEP;
                if (speed < 0.25) {
                    speed = 0.25;
                } else if (speed > 2) {
                    speed = 2;
                }
                snd_pcm_close(pcm_handle);
                snd_pcm_hw_params_free(hw_params);

                setup_snd_pcm(music_names[current_music]);
            }

            if (c == 'n') {
                current_music++;
                current_music %= music_count;

                snd_pcm_close(pcm_handle);
                snd_pcm_hw_params_free(hw_params);
                free(buff);

                open_music_file(music_names[current_music]);
                setup_snd_pcm(music_names[current_music]);
                buff = (unsigned char *)malloc(buffer_size);
            } else if (c == 'b') {
                current_music--;
                current_music = (current_music + music_count) % music_count;

                snd_pcm_close(pcm_handle);
                snd_pcm_hw_params_free(hw_params);
                free(buff);

                open_music_file(music_names[current_music]);
                setup_snd_pcm(music_names[current_music]);
                buff = (unsigned char *)malloc(buffer_size);
            }

            wmove(win, 1, 1);
            wclrtoeol(win);
            mvwprintw(win, 1, 1, "Currently Playing: %s", music_names[current_music]);
            mvwprintw(win, 2, 1, "Current Volume: %f", (volume + 1));
            mvwprintw(win, 3, 1, "Current Speed: %f", (speed));
            wrefresh(win);
        }
        // 读取文件数据放到缓存中
        ret = fread(buff, 1, buffer_size, fp);

        // volume
        int16_t *nbuff = (int16_t *)buff;
        for (size_t i = 0; i < buffer_size / 2; i++) {
            nbuff[i] += volume * nbuff[i];
        }

        if (ret == 0) {
            current_music++;
            current_music %= music_count;

            snd_pcm_close(pcm_handle);
            snd_pcm_hw_params_free(hw_params);
            free(buff);

            open_music_file(music_names[current_music]);
            setup_snd_pcm(music_names[current_music]);
            buff = (unsigned char *)malloc(buffer_size);

            wmove(win, 1, 1);
            wclrtoeol(win);
            mvwprintw(win, 1, 1, "Currently Playing: %s", music_names[current_music]);
            mvwprintw(win, 2, 1, "Current Volume: %f", (volume + 1));
            mvwprintw(win, 3, 1, "Current Speed: %f", (speed));
            wrefresh(win);
            continue;
        }

        if (ret < 0) {
            printf("read pcm from file! \n");
            exit(1);
        }

        // 向PCM设备写入数据, snd_pcm_writei()函数第三个是帧单位，
        while ((ret = snd_pcm_writei(pcm_handle, buff, frames)) < 0) {
            if (ret == -EPIPE) {
                /* EPIPE means underrun -32  的错误就是缓存中的数据不够 */
                printf("underrun occurred -32, err_info = %s \n", snd_strerror(ret));
                //完成硬件参数设置，使设备准备好
                snd_pcm_prepare(pcm_handle);
            } else if (ret < 0) {
                printf("ret value is : %d \n", ret);
                debug_msg(-1, "write to audio interface failed");
            }
        }
    }

    fprintf(stderr, "end of music file input\n");

    // close window
    endwin();
    // 关闭文件
    fclose(fp);
    snd_pcm_close(pcm_handle);

    return 0;
}