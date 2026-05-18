#include <linux/module.h>
#include <linux/printk.h>
#include <linux/kernel.h>
#include <linux/spi/spi.h>
#include <linux/of.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/etherdevice.h>

struct enc_priv {
	struct spi_device *my_spi;
	struct net_device *netdev;
	u8 rx_test_buf[1518];
	u8 spi_transfer_buf[1518];
	u16 next_ptr;
};

static u8 spi_readReg(struct spi_device *spi, u8 address)
{
	u8 tx_buf[2];
	u8 rx_buf[4]; 
	u8 val = 0;
	int slen = 1; 
	int ret;
	
	if(address & 0x80) //set 8th bit for a MAC/MIIM register address
		slen++;

	tx_buf[0] = (0x00 | (address & 0x1f)); //ADDR_MASK 
	
	ret = spi_write_then_read(spi, tx_buf, 1, rx_buf, slen);	
	if(ret) {
		printk(KERN_INFO "readReg failed\n");
	}
	else {
		/*
		 * MAC/MII registers prepend a dummy byte before the real data,
		 * so slen == 2: rx_buf[0] is dummy, rx_buf[1] is the value.
		 * ETH registers return the value directly in rx_buf[0].
		 */
		val = (address & 0x80) ? rx_buf[1] : rx_buf[0];
	}

	return val;
}

static int spi_writeReg(struct spi_device *spi, u8 address, u8 data)
{
	u8 tx_buf[2];
	int ret = 0;

	tx_buf[0] = (0x40 | (address & 0x1f));
	tx_buf[1] = data; 
	
	ret = spi_write(spi, tx_buf, 2);
	if(ret) {
		printk(KERN_INFO "spi-write failed\n");
	}
	return ret;
}

static void enc28j60_set_bank(struct spi_device *spi, int num)
{
    u8 econ1_val;
    econ1_val = spi_readReg(spi, 0x1f);

    econ1_val &= ~0x03;
    econ1_val |= (u8)(num & 0x03);
    spi_writeReg(spi, 0x1f, econ1_val);
}

/* get_revision - read silicon revision ID from EREVID (Bank3, 0x12) */
static u8 get_revision(struct enc_priv *priv)
{
	u8 rev = 0;

	enc28j60_set_bank(priv->my_spi, 3);
	rev = spi_readReg(priv->my_spi, 0x12);

	return rev; 
}

static int spi_read_buf(struct spi_device *spi,
		u16 addr, int len, u8 *rx_buf) 
{
	int ret = 0; 
	u8 tx_buf[1]; 
	tx_buf[0] = 0x3a;

	enc28j60_set_bank(spi, 0); 

	spi_writeReg(spi, 0x00, (addr & 0xff));
	spi_writeReg(spi, 0x01, (addr >> 8)); 

	ret = spi_write_then_read(spi, tx_buf, 1, rx_buf, len);	

	if(ret) {
		printk(KERN_INFO "spi_read_buf failed\n");
	}
	return ret;

}

/*
 * spi_write_buf - write a data buffer to ENC28J60 SRAM via WBM command
 * @priv: driver private data; priv->spi_transfer_buf is used to prepend
 *        the WBM opcode (0x7A) before the payload
 * @len:  number of payload bytes to write
 * @tx_buf: pointer to the data to write
 *
 * The caller is responsible for setting EWRPT before calling this function.
 */
static int spi_write_buf(struct enc_priv *priv, int len, u8 *tx_buf) 
{
	int ret = 0; 

	priv->spi_transfer_buf[0]  = 0x7A;
	memcpy(&priv->spi_transfer_buf[1], tx_buf, len);
	ret = spi_write(priv->my_spi, priv->spi_transfer_buf, len+1);

	if(ret) {
		printk(KERN_INFO "spi_write_buf failed\n");
	}

	return ret; 

}




static u16 erxrdpt_workaround(u16 next_packet_ptr)
{
	u16 erxrdpt;

	if ((next_packet_ptr - 1 < 0x0000) || (next_packet_ptr - 1 > 0x19ff))
		erxrdpt = 0x19FF;
	else
		erxrdpt = next_packet_ptr - 1;

	return erxrdpt;
}

static u16 rx_packet_start(u16 addr) {
	if(addr + 6 > 0x19FF) 
	{
		return (addr + 6) - ((0x19FF - 0x0000) + 1);
		
	}
	else 
		return addr + 6; 
}

/*
 * enc28j60_set_mac_addr - program the randomly generated MAC address into
 *                         the ENC28J60 MAADR registers (Bank3)
 * @priv: driver private data
 *
 * The ENC28J60 stores the MAC address in reverse byte order:
 * MAADR0 holds the last octet, MAADR5 holds the first octet.
 */
static void enc28j60_set_mac_addr(struct enc_priv *priv) {
	struct net_device *dev = priv->netdev;
	struct spi_device *spi = priv->my_spi;

	printk(KERN_INFO "my_enc28j60: Generated Random MAC = %pM\n", dev->dev_addr);

	enc28j60_set_bank(spi, 3);

	/* ENC28J60 MAC address registers are stored in reverse byte order */
	spi_writeReg(spi, 0x00, dev->dev_addr[5]); // MAADR0
	spi_writeReg(spi, 0x01, dev->dev_addr[4]); // MAADR1
	spi_writeReg(spi, 0x02, dev->dev_addr[3]); // MAADR2
	spi_writeReg(spi, 0x03, dev->dev_addr[2]); // MAADR3
	spi_writeReg(spi, 0x04, dev->dev_addr[1]); // MAADR4
	spi_writeReg(spi, 0x05, dev->dev_addr[0]); // MAADR5

	enc28j60_set_bank(spi, 0);
}

static int enc28j60_hw_init(struct enc_priv *priv)
{
	struct spi_device *spi = priv->my_spi;
	u8 bfc[2];

	// 1. 소프트 리셋
	u8 rst = 0xFF;
	spi_write(spi, &rst, 1);
	udelay(2000);

	// 2. RX 버퍼 설정 (Bank0)
	enc28j60_set_bank(spi, 0);
	spi_writeReg(spi, 0x08, 0x00); // ERXSTL
	spi_writeReg(spi, 0x09, 0x00); // ERXSTH
	spi_writeReg(spi, 0x0a, 0xFF); // ERXNDL
	spi_writeReg(spi, 0x0b, 0x19); // ERXNDH  (0x19FF)
	spi_writeReg(spi, 0x0c, 0xFF); // ERXRDPTL (에라타: 홀수)
	spi_writeReg(spi, 0x0d, 0x19); // ERXRDPTH
	spi_writeReg(spi, 0x04, 0x00); // ETXSTL
	spi_writeReg(spi, 0x05, 0x1A); // ETXSTH 

	// 3. MAC 수신 활성화 (Bank2) ← 없으면 PKTIF 절대 안 뜸
	enc28j60_set_bank(spi, 2);
	spi_writeReg(spi, 0x00, 0x0D); // MACON1: MARXEN|TXPAUS|RXPAUS
	spi_writeReg(spi, 0x02, 0xF3); // MACON3: PADCFG|TXCRCEN|FRMLNEN
	spi_writeReg(spi, 0x0a, 0xEE); // MAMXFLL (1518)
	spi_writeReg(spi, 0x0b, 0x05); // MAMXFLH

	// 4. 수신 필터 끄기 - 모든 패킷 수신 (Bank1)
	enc28j60_set_bank(spi, 1);
	spi_writeReg(spi, 0x18, 0x00); // ERXFCON: promiscuous

	// 5. EIR 클리어 후 EIE 설정
	enc28j60_set_bank(spi, 0);
	bfc[0] = 0xA0 | 0x1C; bfc[1] = 0x7F; // BFC EIR
	spi_write(spi, bfc, 2);
	spi_writeReg(spi, 0x1b, 0xC0); // EIE: INTIE|PKTIE

	// 6. RXEN 마지막
	u8 econ1 = spi_readReg(spi, 0x1f);
	spi_writeReg(spi, 0x1f, econ1 | 0x04);

	enc28j60_set_mac_addr(priv);
	return 0;
}

/*
 * enc28j60_tx - transmit a single Ethernet frame
 * @priv: driver private data
 * @len:  frame length in bytes (excluding per-packet control byte)
 * @tx_buf: pointer to frame data
 *
 * TX buffer is fixed at 0x1A00 - 0x1FFF.
 * A per-packet control byte (0x00 = use MACON3 defaults) is written
 * before the frame.  ECON1.TXRTS is set to start transmission.
 */
static int enc28j60_tx(struct enc_priv *priv, int len, u8 *tx_buf) 
{
	struct spi_device *spi = priv->my_spi;
	u8 bfs[2];
	u16 etxnd; 
	u8 control[2];
	control[0] = 0x7a;
	control[1] = 0x00;

	enc28j60_set_bank(spi, 0);	
	spi_writeReg(spi, 0x02, 0x00);	//EWRPTL
	spi_writeReg(spi, 0x03, 0x1A);	//EWRPTH
						
	etxnd = 0x1A00 + len; 
	spi_writeReg(spi, 0x06, etxnd & 0xFF);	//ETXNDL
	spi_writeReg(spi, 0x07, etxnd >> 8);	//ETXNDH 
	
	spi_write(spi, control, 2); //write per-packet control byte 
	spi_write_buf(priv, len, tx_buf);	//SRAM data write 
						//
	//set ECON1.TXRTS, transmit request to send bit from ETXST to ETXND 
	bfs[0] = 0x80 | 0x1F;		
	bfs[1] = 0x08;
	spi_write(spi, bfs, 2);

	return 0;
}


static int enc28j60_rx(struct enc_priv *priv) 
{
	struct spi_device *spi = priv->my_spi;
	u16 next_pk_ptr;
	int i, ret, len; 
	ret = 0;
	u8 rsv[4]; 
	u16 erxrdpt;
	u8 bfs[2];

	if(spi_read_buf(spi, priv->next_ptr, 4, rsv)) {
		printk(KERN_DEBUG "enc28j60_rx failed to read buf_1\n");
		return -1; 
	}

	next_pk_ptr = rsv[1];
	next_pk_ptr <<= 8;
	next_pk_ptr |=  rsv[0];
	
	len = rsv[3];
	len <<= 8;
	len |= rsv[2];	

	if(spi_read_buf(spi, rx_packet_start(priv->next_ptr), len, priv->rx_test_buf)) {		
		printk(KERN_DEBUG "enc28j60_rx failed to read buf_2\n");
		return -1;  
	}

	enc28j60_set_bank(spi, 0);

	erxrdpt = erxrdpt_workaround(next_pk_ptr);
	spi_writeReg(spi, 0x0c, (u8)(erxrdpt & 0xFF));	//ERXRDPTL
	spi_writeReg(spi, 0x0d, (u8)(erxrdpt >> 8));	//ERXRDPTH 
							//
	//PKTDEC by using BFS to protect AUTOINC bit. 
	bfs[0] = 0x80 | 0x1E;
	bfs[1] = 0x40;
	spi_write(spi, bfs, 2);

	priv->next_ptr = next_pk_ptr;

	printk(KERN_INFO "ENC28J60: RX Interrupt Checking: ");
	print_hex_dump(KERN_INFO, "ENC28J60: RX Packet: ", DUMP_PREFIX_OFFSET, 16, 1, priv->rx_test_buf, len, true);
	return ret; 
}

static int enc28j60_rx_interrupt(struct enc_priv *priv)
{
	struct spi_device *spi = priv->my_spi;
	int pkt_cnt;

	enc28j60_set_bank(spi, 1);
	pkt_cnt = (int)spi_readReg(spi, 0x19);
	printk(KERN_INFO "pkt_cnt = %d\n", pkt_cnt); 

	enc28j60_set_bank(spi, 0);


	while(pkt_cnt-- > 0) {
	//	spi_writeReg(spi, 0x1E, 0x40);  //write 1 to ECON2.PKTDEC to decrement
	//	printk(KERN_INFO "pkt_cnt after Decrement attempt = %d\n", pkt_cnt); 
		enc28j60_rx(priv);
	}
	return pkt_cnt;
}

static irqreturn_t enc28j60_irq_thread(int irq, void *dev_id) 
{
	int loop;
	u8 pkt_cnt;
	/* dev_id is priv, registered in probe via request_threaded_irq() */
	struct enc_priv *priv = (struct enc_priv *)dev_id;
	struct spi_device *spi = priv->my_spi;
	u8 intflag = spi_readReg(spi, 0x1c);
	u8 bfc[2];

	printk(KERN_INFO "my_enc28j60: Int Flag: 0x%02x\n", intflag);
	
	printk(KERN_INFO "my_enc28j60: [★★★ IRQ THREAD WOKE UP! ★★★]\n");

	enc28j60_rx_interrupt(priv);

/*	do {
		loop = 0;
		if((intflag & 0x01)  != 0) {
			loop++;
			printk(KERN_DEBUG "my_enc28j60_custom: RX error Int\n");
		}
		if((intflag & 0x02)  != 0) {
			loop++;
			printk(KERN_DEBUG "my_enc28j60_custom: TX error Int\n");
		}
		if((intflag & 0x08)  != 0) {
			loop++;
			printk(KERN_DEBUG "my_enc28j60_custom: TX Int\n");
		}
		if((intflag & 0x10)  != 0) {
			loop++;
			printk(KERN_DEBUG "my_enc28j60_custom: Link Int\n");
		}
		if((intflag & 0x20)!= 0) {
			loop++;
			printk(KERN_DEBUG "my_enc28j60_custom: DMA error Int\n");
		}
		if((intflag & 0x40)!= 0) {
			loop++;
			printk(KERN_DEBUG "my_enc28j60_custom: PKT Int\n");
		}

		if(enc28j60_rx_interrupt(spi)) {
			loop++;
		}
					

	} while(loop);	*/

	//clear EIR flags 
	bfc[0] = 0xA0 | 0x1C;
	bfc[1] = intflag & 0x7F;
	spi_write(spi, bfc, 2);
	
	return IRQ_HANDLED;
}

/*
 * enc28j60_start_xmit - ndo_start_xmit handler for net_device_ops
 * @skb: socket buffer handed down from the network stack
 * @dev: net_device associated with this driver instance
 *
 * Passes the raw frame to enc28j60_tx() and frees the skb.
 * TX statistics are updated on each call.
 */
static netdev_tx_t enc28j60_start_xmit(struct sk_buff *skb,
					struct net_device *dev)
{
	struct enc_priv *priv = netdev_priv(dev);

	enc28j60_tx(priv, skb->len, skb->data);

	dev->stats.tx_packets++;
	dev->stats.tx_bytes += skb->len;

	dev_kfree_skb(skb);
	return NETDEV_TX_OK;
}

static const struct net_device_ops enc28j60_netdev_ops = {
	.ndo_start_xmit = enc28j60_start_xmit,
};

/**
 * my_enc28j60_probe - Bind the driver to an SPI device
 */
static int my_enc28j60_probe(struct spi_device *spi)
{
	u8 rev; 
	int ret; 

	struct net_device *dev; 
	struct enc_priv *priv; 

	dev = alloc_etherdev(sizeof(struct enc_priv));
	if (!dev) {
		ret = -ENOMEM;
	}

	priv = netdev_priv(dev);

	priv->netdev = dev;	/* priv to netdev reference */
	priv->my_spi = spi;	/* priv to spi reference */

	/* store priv in spi drvdata so my_enc28j60_remove() can retrieve it */
	spi_set_drvdata(spi, priv);

	dev->irq = spi->irq;
	dev->netdev_ops = &enc28j60_netdev_ops;

	eth_hw_addr_random(dev);


	printk(KERN_INFO "======================================\n");
	printk(KERN_INFO "My Driver: Probe function called!\n");
	printk(KERN_INFO "My Driver: Matching with compatible 'my,enc28j60' SUCCESS!\n");
	printk(KERN_INFO "======================================\n");

	rev = get_revision(priv);	
	printk(KERN_INFO "Revision Number: %x\n", rev);
	

	printk(KERN_INFO "probe called, spi->irq = %d\n", spi->irq);

	/* hw_init also calls enc28j60_set_mac_addr() as its last step */
	enc28j60_hw_init(priv); 

	/* pass priv as dev_id so enc28j60_irq_thread() can reach all state */
	ret = request_threaded_irq(spi->irq, NULL, enc28j60_irq_thread, 
		IRQF_ONESHOT, "my_enc28j60", priv);

	priv->next_ptr = 0x0000;	//start pointer 
					//
					//
	ret = register_netdev(dev);

	return ret; 
}

/**
 * my_enc28j60_remove - Unbind the driver from the SPI device
 */
static int my_enc28j60_remove(struct spi_device *spi)
{
	struct enc_priv *priv = spi_get_drvdata(spi);
	unregister_netdev(priv->netdev);
	free_irq(spi->irq, priv);	/* dev_id must match what probe passed */
	free_netdev(priv->netdev);
	printk(KERN_INFO "My Driver: Remove function called. Goodbye!\n");
	return 0;
}

/* Device Tree matching table */
static const struct of_device_id my_enc28j60_dt_ids[] = {
	{ .compatible = "my,enc28j60_custom", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, my_enc28j60_dt_ids);

static struct spi_driver my_enc28j60_driver = {
	.driver = {
		.name = "my_enc28j60",
		.of_match_table = my_enc28j60_dt_ids,
	},
	.probe = my_enc28j60_probe,
	.remove = my_enc28j60_remove,
};

static int __init my_enc28j60_init(void)
{
	printk(KERN_INFO "My Driver: Initializing SPI driver\n");
	return spi_register_driver(&my_enc28j60_driver);
}

static void __exit my_enc28j60_exit(void)
{
	
	spi_unregister_driver(&my_enc28j60_driver);
	printk(KERN_INFO "My Driver: Driver unregistered\n");
}

module_init(my_enc28j60_init);
module_exit(my_enc28j60_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SonHeeSeung");
MODULE_DESCRIPTION("SPI probe skeleton driver");
