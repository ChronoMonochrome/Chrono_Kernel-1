/*
 * Copyright (C) ST-Ericsson SA 2011
 *
 * Author: Chris Blair <chris.blair@stericsson.com> for ST-Ericsson
 *   based on modem_shrm_driver.c
 *
 * License terms: GNU General Public License (GPL) version 2
 *
 * SPI driver implementing the M6718 inter-processor communication protocol.
 */
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/spi/spi.h>
#include <linux/modem/modem_client.h>
#include <linux/modem/m6718_spi/modem_driver.h>

static struct modem_spi_dev modem_driver_data = {
	.dev = NULL,
	.ndev = NULL,
	.modem = NULL,
	.isa_context = NULL,
	.netdev_flag_up = 0
};

/**
 * modem_m6718_spi_receive() - Receive a frame from L1 physical layer
 * @sdev:	pointer to spi device structure
 * @channel:	L2 mux channel id
 * @len:	frame data length
 * @data:	pointer to frame data
 *
 * This function is called from the driver L1 physical transport layer. It
 * copied the frame data to the receive queue for the channel on which the data
 * was received.
 *
 * Special handling is given to slave-loopback channels where the data is simply
 * sent back to the modem on the same channel.
 *
 * Special handling is given to the ISI channel when PHONET is enabled - the
 * phonet tasklet is scheduled in order to pump the received data through the
 * net device interface.
 */
int modem_m6718_spi_receive(struct spi_device *sdev, u8 channel,
	u32 len, void *data)
{
	return -ENODEV;
}
EXPORT_SYMBOL_GPL(modem_m6718_spi_receive);

static int spi_probe(struct spi_device *sdev)
{
	int result = 0;

	spi_set_drvdata(sdev, &modem_driver_data);

	/*
	 * Since we can have multiple spi links for the same modem, only
	 * initialise the modem data and char/net interfaces once.
	 */
	if (modem_driver_data.dev == NULL) {
		modem_driver_data.dev = &sdev->dev;
		modem_driver_data.modem =
			modem_get(modem_driver_data.dev, "m6718");
		if (modem_driver_data.modem == NULL) {
			dev_err(&sdev->dev,
				"failed to retrieve modem description\n");
			result = -ENODEV;
		}
	}
	return result;
}

static int __exit spi_remove(struct spi_device *sdev)
{
	return 0;
}

#ifdef CONFIG_PM
/**
 * spi_suspend() - This routine puts the IPC driver in to suspend state.
 * @sdev: pointer to spi device structure.
 * @mesg: pm operation
 *
 * This routine checks the current ongoing communication with modem
 * and prevents suspend if modem communication is on-going.
 */
static int spi_suspend(struct spi_device *sdev, pm_message_t mesg)
{
	dev_dbg(&sdev->dev, "suspend called\n");
	return 0;
}

/**
 * spi_resume() - This routine resumes the IPC driver from suspend state.
 * @sdev: pointer to spi device structure
 */
static int spi_resume(struct spi_device *sdev)
{
	dev_dbg(&sdev->dev, "resume called\n");
	return 0;
}
#endif /* CONFIG_PM */

static struct spi_driver spi_driver = {
	.driver = {
		.name = "spimodem",
		.bus  = &spi_bus_type,
		.owner = THIS_MODULE
	},
	.probe = spi_probe,
	.remove = __exit_p(spi_remove),
#ifdef CONFIG_PM
	.suspend = spi_suspend,
	.resume = spi_resume,
#endif
};

static int __init m6718_spi_driver_init(void)
{
	pr_info("M6718 modem driver initialising\n");
	return spi_register_driver(&spi_driver);
}
module_init(m6718_spi_driver_init);

static void __exit m6718_spi_driver_exit(void)
{
	pr_debug("M6718 modem SPI IPC driver exit\n");
	spi_unregister_driver(&spi_driver);
}
module_exit(m6718_spi_driver_exit);

MODULE_AUTHOR("Chris Blair <chris.blair@stericsson.com>");
MODULE_DESCRIPTION("M6718 modem IPC SPI driver");
MODULE_LICENSE("GPL v2");
