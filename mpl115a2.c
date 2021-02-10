/*!
 *
 * Receive and print the value of pressure from MPL115A2 sold by Akizuki-denshi.
 *
 * @file mpl115a2.c
 *
 * @date 2014/08/22
 * @author Kodai Tooi
 * @version 1.0
 */
#include "lib/i2c-ctl.h"
#include <unistd.h>

#if MODULE
  #include <linux/kernel.h>
  #include <linux/module.h>
  #include <linux/errno.h>
  #include <linux/fs.h>
#else
  #include <string.h>
  #include <stdio.h>
  #include <stdlib.h>
  //ユーザランドでも動くようにするための、関数・定数の再定義
  #define printk(...) fprintf(stderr, __VA_ARGS__)
  #define KERN_INFO ""
  #define KERN_NOTICE ""
  #define KERN_WARNING ""
  #define KERN_ERR ""
#endif

#define I2C_DEV "/dev/i2c-%d"

#define MPL115A2_ID 0x60
#define MPL115A2_DEV_NAME "mpl115a2"
#define I2C_SLAVE_MAX_RETRY 5
#define MPL115A2_WAIT_READMODE 3000   // 1600 to 3000

struct mpl115a2 {

  char register_data[16];
  double a0;
  double b1;
  double b2;
  double c12;
  int padc;
  int tadc;
  double c12x2;
  double a1;
  double a1x1;
  double y1;
  double a2x2;
  double pcomp;
  double pressure;
  // register_data の取得時刻とか。
};

#if MODULE
static struct mpl115a2 *cur_data;
#endif

float convert_coefficient(char msb, char lsb, int total_bits, int fractional_bits, int zero_pad) {

  float result;
  unsigned short bytes;
  short sign = 1;

  bytes = (msb << 8) | lsb;
  if ((msb >> 7) == 1) {
    bytes = (0xffff ^ bytes) + 1;
    sign = -1;
  }

  result = bytes / (float)(1 << (16 - total_bits + fractional_bits + zero_pad));
  return sign * result;
}

/*!
 * @brief Check the error of coefficient values from MPL115A2.
 *
 * @param[out] mpl115a2_data Collect the data to this object from MPL115A2.
 *
 * @return Successed : 0, Failed : -1
 */
int check_coefficient_err(struct mpl115a2 *mpl115a2_data) {

  if (
    mpl115a2_data->register_data[12] != 0 ||
    mpl115a2_data->register_data[13] != 0 ||
    mpl115a2_data->register_data[14] != 0 ||
    mpl115a2_data->register_data[15] != 0
  ) {
    printk(KERN_WARNING "mpl115a2 : Error detected when get coefficient from mpl115a2. (%02x, %02x, %02x, %02x)\n", mpl115a2_data->register_data[12], mpl115a2_data->register_data[13], mpl115a2_data->register_data[14], mpl115a2_data->register_data[15]);
    return -1;
  }
  return 0;
}

/*!
 * @brief Check the error of measured values from MPL115A2.
 *
 * @param[out] mpl115a2_data Collect the data to this object from MPL115A2.
 *
 * @return Successed : 0, Failed : -1
 */
int check_measure_err(struct mpl115a2 *mpl115a2_data) {

  int ret = 0;

  if (
    (mpl115a2_data->register_data[0] == 0x00 && mpl115a2_data->register_data[1] == 0x00) ||
    (mpl115a2_data->register_data[0] == 0xff && mpl115a2_data->register_data[1] == 0xff)
  ) {
    printk(KERN_WARNING "mpl115a2 : Error detected when measure from mpl115a2. (padc : %02x, %02x)\n", mpl115a2_data->register_data[0], mpl115a2_data->register_data[1]);
    ret = -1;
  }
  if (
    (mpl115a2_data->register_data[2] == 0x00 && mpl115a2_data->register_data[3] == 0x00) ||
    (mpl115a2_data->register_data[2] == 0xff && mpl115a2_data->register_data[3] == 0xff)
  ) {
    printk(KERN_WARNING "mpl115a2 : Error detected when measure from mpl115a2. (tadc : %02x, %02x)\n", mpl115a2_data->register_data[2], mpl115a2_data->register_data[3]);
    ret = -1;
  }
  return ret;
}

/*!
 * @brief Get the value of coefficient from MPL115A2.
 *
 * @param[out] mpl115a2_data Collect the data to this object from MPL115A2.
 *
 * @return Successed : 0, Failed : -1
 */
int get_coefficient(struct mpl115a2 *mpl115a2_data) {

  char i2c_dev_name[64], write_data[2];
  I2CSlave *mpl115a2;

  sprintf(i2c_dev_name, I2C_DEV, 1);
  mpl115a2 = gen_i2c_slave(i2c_dev_name, MPL115A2_DEV_NAME, MPL115A2_ID, 5, 3000);
  if (init_i2c_slave(mpl115a2) == -1) {
    return -1;
  }

  write_data[0] = 0x04; // Top of coefficient address for read register of MPL115A2.
  if (write_i2c_slave(mpl115a2, write_data, 1) == -1) {
    return -1;
  }
  if (read_i2c_slave(mpl115a2, &(mpl115a2_data->register_data[4]), 12) == -1) {
    return -1;
  }

  if (term_i2c_slave(mpl115a2) == -1) {
    return -1;
  }

  if (destroy_i2c_slave(mpl115a2) == -1) {
    return -1;
  }

  if (check_coefficient_err(mpl115a2_data) == -1) {
    return -1;
  }

//mpl115a2_data->register_data[4] = 0x3e;
//mpl115a2_data->register_data[5] = 0xce;
//mpl115a2_data->register_data[6] = 0xb3;
//mpl115a2_data->register_data[7] = 0xf9;
//mpl115a2_data->register_data[8] = 0xc5;
//mpl115a2_data->register_data[9] = 0x17;
//mpl115a2_data->register_data[10] = 0x33;
//mpl115a2_data->register_data[11] = 0xc8;

  mpl115a2_data->a0 = convert_coefficient(mpl115a2_data->register_data[4], mpl115a2_data->register_data[5], 16, 3, 0);
  mpl115a2_data->b1 = convert_coefficient(mpl115a2_data->register_data[6], mpl115a2_data->register_data[7], 16, 13, 0);
  mpl115a2_data->b2 = convert_coefficient(mpl115a2_data->register_data[8], mpl115a2_data->register_data[9], 16, 14, 0);
  mpl115a2_data->c12 = convert_coefficient(mpl115a2_data->register_data[10], mpl115a2_data->register_data[11], 14, 13, 9);
  return 0;
}

/*!
 * @brief Measure the value of pressure from MPL115A2.
 *
 * @param[out] mpl115a2_data Collect the data to this object from MPL115A2.
 *
 * @return Successed : 0, Failed : -1
 */
int measure(struct mpl115a2 *mpl115a2_data) {

  char i2c_dev_name[64], write_data[2];
  I2CSlave *mpl115a2;

  sprintf(i2c_dev_name, I2C_DEV, 1);
  mpl115a2 = gen_i2c_slave(i2c_dev_name, MPL115A2_DEV_NAME, MPL115A2_ID, 5, 3000);
  if (init_i2c_slave(mpl115a2) == -1) {
    return -1;
  }

  // Step.1
  write_data[0] = 0x12; // Start conversion.
  write_data[1] = 0x00; // Start conversion.
  if (write_i2c_slave(mpl115a2, write_data, 2) == -1) {
    return -1;
  }

  usleep(MPL115A2_WAIT_READMODE);
  write_data[0] = 0x00; // Top of address for read register of MPL115A2.
  if (write_i2c_slave(mpl115a2, write_data, 1) == -1) {
    return -1;
  }

  // Step 3 : Recive data from MPL115A2.
  if (read_i2c_slave(mpl115a2, mpl115a2_data->register_data, 4) == -1) {
    return -1;
  }

  if (term_i2c_slave(mpl115a2) == -1) {
    return -1;
  }

  if (destroy_i2c_slave(mpl115a2) == -1) {
    return -1;
  }

  if (check_measure_err(mpl115a2_data) == -1) {
    return -1;
  }

//mpl115a2_data->register_data[0] = 0x66;
//mpl115a2_data->register_data[1] = 0x80;
//mpl115a2_data->register_data[2] = 0x7e;
//mpl115a2_data->register_data[3] = 0xc0;

  mpl115a2_data->padc = (mpl115a2_data->register_data[0] << 8 | mpl115a2_data->register_data[1]) >> 6;
  mpl115a2_data->tadc = (mpl115a2_data->register_data[2] << 8 | mpl115a2_data->register_data[3]) >> 6;
  return 0;
}

void calc_pressure(struct mpl115a2* mpl115a2_data) {

  mpl115a2_data->c12x2 = mpl115a2_data->c12 * mpl115a2_data->tadc;
  mpl115a2_data->a1 = mpl115a2_data->b1 + mpl115a2_data->c12x2;
  mpl115a2_data->a1x1 = mpl115a2_data->a1 * mpl115a2_data->padc;
  mpl115a2_data->y1 = mpl115a2_data->a0 + mpl115a2_data->a1x1;
  mpl115a2_data->a2x2 = mpl115a2_data->b2 * mpl115a2_data->tadc;
  mpl115a2_data->pcomp = mpl115a2_data->y1 + mpl115a2_data->a2x2;
  mpl115a2_data->pressure = (mpl115a2_data->pcomp * 65 /1023) + 50;
}

#if MODULE
int init_module(void) {

  if ( register_chrdev(0x2321, "mpl115a2_humidity", &mpl115a2_humidity_fops ) ) {
    printk(KERN_INFO "mpl115a2_humidity : louise chan ha genjitsu ja nai!?\n" );
    return -EBUSY;
  }
  if (register_chrdev(0x2322, "mpl115a2_temperature", &mpl115a2_temperature_fops ) ) {
    printk( KERN_INFO "mpl115a2_temperature : louise chan ha genjitsu ja nai!?\n" );
    return -EBUSY;
  }
  cur_data = kmalloc(sizeof(struct mpl115a2_data), GFP_KERNEL);

  return 0;
}

void cleanup_module(void){

  unregister_chrdev(0x2321, "mpl115a2_humidity");
  unregister_chrdev(0x2322, "mpl115a2_temperature");
  kfree(cur_data);
  printk(KERN_INFO "mpl115a2 : Harukeginia no louise he todoke !!\n" );
}
#else

int measure_retry(struct mpl115a2* mpl115a2_data) {

  int count = 0;

  while (get_coefficient(mpl115a2_data) == -1) {
    if (I2C_SLAVE_MAX_RETRY < ++count) {
      printk(KERN_WARNING "mpl115a2 : Failed get coefficient from mpl115a2.\n");
      return -1;
    }
    printk(KERN_NOTICE "mpl115a2 : Failed get coefficient from mpl115a2. retry %d of %d\n", count, I2C_SLAVE_MAX_RETRY);
    usleep(1000000);
  }

  count = 0;
  while (measure(mpl115a2_data) == -1) {
    if (I2C_SLAVE_MAX_RETRY < ++count) {
      printk(KERN_WARNING "mpl115a2 : Failed measure from mpl115a2.\n");
      return -1;
    }
    printk(KERN_NOTICE "mpl115a2 : Failed measure from mpl115a2. retry %d of %d\n", count, I2C_SLAVE_MAX_RETRY);
    usleep(1000000);
  }
  calc_pressure(mpl115a2_data);

  return 0;
}

void print_help(void) {

  printf("Usage: mpl115a2 [OPTION]\n");
  printf("Receive the data from MPL115A2 which is I2C Slave device and print the value of temperature, humidity, discomfort index.\n\n");
  printf("  -c\tPrint the value in CSV format.\n");
  printf("  -j\tPrint the value in JSON format.\n");
  printf("  -r\tPrint the value in human readable format.\n");
  printf("  -h\tShow this message.\n\n");
  printf("Report bugs to mrkoh_t.bug-report@mem-notfound.net\n");
}

int main(int argc, char* argv[]) {

  int arg, format;
  struct mpl115a2 mpl115a2_data;

  while ((arg = getopt(argc, argv, "cjrh")) != -1) {
    format = arg;
  }
  if (format == 'h') {
    print_help();
    return 1;
  }
  if (measure_retry(&mpl115a2_data) == -1) {
    printf("Failed measure data from MPL115A2.\n");
  } else {
    switch(format) {
      case 'c':
        printf("%.1f,%.1f,%.1f,%1f,%d,%d,%.1f\n"
          , mpl115a2_data.a0
          , mpl115a2_data.b1
          , mpl115a2_data.b2
          , mpl115a2_data.c12
          , mpl115a2_data.padc
          , mpl115a2_data.tadc
          , mpl115a2_data.pressure * 10
        );
        break;
      case 'j':
        printf("{\"a0\":%.1f,\"b1\":%.1f,\"b2\":%.1f,\"c12\":%.1f,\"padc\":%d,\"tadc\":%d,\"hPa\":%.1f}\n"
          , mpl115a2_data.a0
          , mpl115a2_data.b1
          , mpl115a2_data.b2
          , mpl115a2_data.c12
          , mpl115a2_data.padc
          , mpl115a2_data.tadc
          , mpl115a2_data.pressure * 10
        );
        break;
      case 'r':
      default:
        printf("a0   : %f\nb1   : %f\nb2   : %f\nc12  : %f\npadc : %d\ntadc : %d\nhPa  : %f\n"
          , mpl115a2_data.a0
          , mpl115a2_data.b1
          , mpl115a2_data.b2
          , mpl115a2_data.c12
          , mpl115a2_data.padc
          , mpl115a2_data.tadc
          , mpl115a2_data.pressure * 10
        );
        break;
    }
  }

  return 0;
}
#endif

