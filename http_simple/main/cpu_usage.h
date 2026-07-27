#ifndef CPU_USAGE_H
#define CPU_USAGE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CPU使用率監視を開始する。
 *
 * 内部でCPU使用率を定期的に計測するFreeRTOSタスクを生成する。
 * app_main()から起動時に1回だけ呼び出す。
 */
void cpu_usage_init(void);


/*
 * ESP32全体のCPU使用率を取得する。
 *
 * Core 0とCore 1の平均値。
 *
 * 戻り値:
 *   0.0 ～ 100.0 [%]
 */
float cpu_usage_get_total(void);


/*
 * Core 0のCPU使用率を取得する。
 *
 * 戻り値:
 *   0.0 ～ 100.0 [%]
 */
float cpu_usage_get_core0(void);


/*
 * Core 1のCPU使用率を取得する。
 *
 * 戻り値:
 *   0.0 ～ 100.0 [%]
 */
float cpu_usage_get_core1(void);


#ifdef __cplusplus
}
#endif

#endif /* CPU_USAGE_H */