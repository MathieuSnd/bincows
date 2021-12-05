

/**
 * PCIE drivers interfaces:
 * 
 * provide void init(void* config_space_base)
 * 
 * functions to call:
 *      register_irq(unsigned)
 *      unregister_irq(unsigned)
 * 
 *      int  register_timer(void callback(void), unsigned period)
 *      void unregister_timer(int timer_id)
 *     
 */



void 
/**
 * return 0 if the irq was successfully installed
 */
int register_irq(uint8_t irq_number, );