

#define TEST_RESET_AUTOADDR		(0x01 << 0)
#define TEST_BIST				(0x01 << 1)
#define TEST_SINGLE_WORK		(0x01 << 2)
#define TEST_MULIPLE_WORK		(0x01 << 3)

#define DUMMY_BYTES			0
#define	BCAST_CHIP_ID		0x00
#define CMD_LEN				1
#define	CHIP_ID_LEN			1
#define	TARGET_LEN			6
#define NONCE_LEN			4
#define	HASH_LEN			32
#define MIDSTATE_LEN		32
// MERKLEROOT + TIMESTAMP + DIFFICULTY
#define DATA_LEN			12
#define DISABLE_LEN			32
#define READ_RESULT_LEN 	18

#define ASIC_BOOST_CORE_NUM		4
#define TOTAL_MIDSTATE_LEN	(MIDSTATE_LEN * ASIC_BOOST_CORE_NUM)
#define TOTAL_HASH_LEN		(HASH_LEN * ASIC_BOOST_CORE_NUM)

// midstate + data + midstate + midstate + midstate
#define WRITE_JOB_LEN		((TOTAL_MIDSTATE_LEN + DATA_LEN))
#define MAX_CMD_LEN			142

#define MAX_PARM_LEN        32

#define GPIO_SYSFS			"/sys/class/gpio"
#define GPIO_EXPORT			"/export"
#define GPIO_DIR			"/direction"
#define GPIO_VALUE			"/value"
#define GPIO_DIR_IN			"in"
#define GPIO_DIR_OUT		"out"
#define MAX_BUF				128

#define OON_IRQ_ENB			(1 << 4)
#define LAST_CHIP_FLAG		(1 << 15)
#define ASIC_BOOST_EN		(1 << 9)

#define _6818_FPGA_GPIO_RESET      127
#define _6818_FPGA_GPIO_IRQ_OON    125
#define _6818_FPGA_GPIO_IRQ_GN     126

#define _3220_FPGA_GPIO_RESET      66
#define _3220_FPGA_GPIO_IRQ_OON    65
#define _3220_FPGA_GPIO_IRQ_GN     113

#define DEFAULT_UDIV		0x03

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define ALIGN(x, a) (((x) + (a) - 1) & ~((a) - 1))

#define FUNC_IN()			printf("\n ================== %s() ==================\n", __FUNCTION__)


enum BTC08_cmd {
	SPI_CMD_READ_ID			= 0x00,
	SPI_CMD_AUTO_ADDRESS	= 0x01,
	SPI_CMD_RUN_BIST		= 0x02,
	SPI_CMD_READ_BIST		= 0x03,
	SPI_CMD_RESET			= 0x04,
	SPI_CMD_SET_PLL_CONFIG	= 0x05, /* noresponse */
	SPI_CMD_READ_PLL		= 0x06,
	SPI_CMD_WRITE_PARM		= 0x07,
	SPI_CMD_READ_PARM		= 0x08,
	SPI_CMD_WRITE_TARGET	= 0x09,
	SPI_CMD_READ_TARGET		= 0x0A,
	SPI_CMD_RUN_JOB			= 0x0B,
	SPI_CMD_READ_JOB_ID		= 0x0C,
	SPI_CMD_READ_RESULT		= 0x0D,
	SPI_CMD_CLEAR_OON		= 0x0E,
	SPI_CMD_SET_DISABLE		= 0x10,
	SPI_CMD_READ_DISABLE	= 0x11,
	SPI_CMD_SET_CONTROL		= 0x12,	/* no response */
	SPI_CMD_READ_TEMP		= 0x14,
	SPI_CMD_WRITE_NONCE		= 0x16,
	SPI_CMD_READ_HASH		= 0x20,
	SPI_CMD_READ_FEATURE	= 0x32,
	SPI_CMD_READ_REVISION	= 0x33,
	SPI_CMD_SET_PLL_FOUT_EN	= 0x34,
	SPI_CMD_SET_PLL_RESETB 	= 0x35,
};

static int cmd_READ_ID (int fd, uint8_t chip_id);
static uint8_t cmd_AUTO_ADDRESS (int fd);
static void cmd_RUN_BIST (int fd, uint8_t *golden_hash_upper, uint8_t *golden_hash_lower,
						uint8_t *golden_hash_lower2, uint8_t *golden_hash_lower3);
static uint8_t cmd_READ_BIST (int fd, uint8_t chip_id);
static void cmd_RESET (int fd);
static void cmd_SET_PLL_CONFIG (int fd, uint8_t chip_id, int pll_idx);
static void cmd_READ_PLL (int fd, uint8_t chip_id);
static void cmd_WRITE_PARM (int fd, uint8_t chip_id, uint8_t *midstate, uint8_t *data);
static void cmd_READ_PARM (int fd, uint8_t chip_id);
static void cmd_WRITE_TARGET (int fd, uint8_t chip_id, uint8_t *target);
static void cmd_READ_TARGET (int fd, uint8_t chip_id);
static void cmd_RUN_JOB (int fd, uint8_t chip_id, uint8_t job_id);
static void cmd_READ_JOB_ID (int fd, uint8_t chip_id);
static void cmd_READ_RESULT (int fd, uint8_t chip_id);
static void cmd_CLEAR_OON (int fd, uint8_t chip_id);
static void cmd_SET_DISABLE (int fd, uint8_t chip_id, uint8_t* disable);
static void cmd_READ_DISABLE (int fd, uint8_t chip_id);
static void cmd_SET_CONTROL (int fd, uint8_t chip_id, uint32_t udiv);
static void cmd_READ_TEMP (int fd, uint8_t chip_id);
static void cmd_WRITE_NONCE (int fd, uint8_t chip_id, uint8_t *start_nonce, uint8_t *end_nonce);
static void cmd_READ_HASH (int fd, uint8_t chip_id);
static void cmd_READ_FEATURE (int fd, uint8_t chip_id);
static void cmd_READ_REVISION (int fd, uint8_t chip_id);
static void cmd_SET_PLL_FOUT_EN (int fd, uint8_t chip_id, uint8_t fout_en);
static void cmd_SET_PLL_RESETB (int fd, uint8_t chip_id, uint8_t reset);

static int seq_reset_autoaddr (int fd);
static int seq_bist (int fd);
static int seq_workloop (int fd);

struct pll_conf {
	int freq;
	union {
		struct {
			int p        : 6;
			int m        :10;
			int s        : 3;
			int bypass   : 1;
			int div_sel  : 1;
			int afc_enb  : 1;
			int extafc   : 5;
			int feed_en  : 1;
			int fsel     : 1;
			int rsvd     : 3;
		};
		unsigned int val;
	};
};

static struct pll_conf pll_sets[] = {
	{ 300, {6, 600, 2, 0, 1, 0, 0, 0, 0, 0}},
	{ 350, {6, 700, 2, 0, 1, 0, 0, 0, 0, 0}},
	{ 400, {6, 400, 1, 0, 1, 0, 0, 0, 0, 0}},
	{ 450, {6, 450, 1, 0, 1, 0, 0, 0, 0, 0}},
	{ 500, {6, 500, 1, 0, 1, 0, 0, 0, 0, 0}},
	{ 550, {6, 550, 1, 0, 1, 0, 0, 0, 0, 0}},
	{ 600, {6, 600, 1, 0, 1, 0, 0, 0, 0, 0}},
	{ 650, {6, 650, 1, 0, 1, 0, 0, 0, 0, 0}},
	{ 700, {6, 700, 1, 0, 1, 0, 0, 0, 0, 0}},
	{ 750, {6, 750, 1, 0, 1, 0, 0, 0, 0, 0}},
	{ 800, {6, 800, 1, 0, 1, 0, 0, 0, 0, 0}},
	{ 850, {6, 425, 0, 0, 1, 0, 0, 0, 0, 0}},
	{ 900, {6, 450, 0, 0, 1, 0, 0, 0, 0, 0}},
	{ 950, {6, 475, 0, 0, 1, 0, 0, 0, 0, 0}},
	{1000, {6, 500, 0, 0, 1, 0, 0, 0, 0, 0}},
};

/* GOLD_MIDSTATE */
static uint8_t default_golden_midstate[MIDSTATE_LEN] = {
	0x5f, 0x4d, 0x60, 0xa2, 0x53, 0x85, 0xc4, 0x07, 
	0xc2, 0xa8, 0x4e, 0x0c, 0x25, 0x91, 0x69, 0xc4, 
	0x10, 0xa4, 0xa5, 0x4b, 0x93, 0xf7, 0x17, 0x08, 
	0xf1, 0xab, 0xdf, 0xec, 0x6e, 0x8b, 0x81, 0xd2,
};

/* GOLD_DATA */
static uint8_t default_golden_data[DATA_LEN] = {
	/* Data (MerkleRoot, Time, Target) */
	0xf4, 0x2a, 0x1d, 0x6e, 0x5b, 0x30, 0x70, 0x7e, 
	0x17, 0x37, 0x6f, 0x56,
};

/* GOLD_START_NONCE */
static uint8_t default_golden_start_nonce[NONCE_LEN] = {
	0x66, 0xcb, 0x34, 0x20
};

static uint8_t default_golden_end_nonce[NONCE_LEN] = {
	0x66, 0xcb, 0x34, 0x30
};

/* GOLD_TARGET */
static uint8_t default_golden_target[TARGET_LEN] = {
	0x17, 0x37, 0x6f, 0x56, 0x05, 0x00
};

static uint8_t default_golden_hash[HASH_LEN] = {
	/* GOLD_HASH */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x22, 0x09, 0x3d, 0xd4, 0x38, 0xed, 0x47,
	0xfa, 0x28, 0xe7, 0x18, 0x58, 0xb8, 0x22, 0x0d,
	0x53, 0xe5, 0xcd, 0x83, 0xb8, 0xd0, 0xd4, 0x42,
};
