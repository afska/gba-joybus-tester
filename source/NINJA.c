#include <fat.h>
#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wiiuse/wpad.h>

static void* xfb = NULL;
static GXRModeObj* rmode = NULL;

vu32* const si_reg = (u32*)0xCD006400;
vu32* const si_buf = (u32*)0xCD006480;

u32 swap_bytes(u32 input) {
  u32 result = 0;

  result |= ((input & 0xFF000000) >> 24);
  result |= ((input & 0x00FF0000) >> 8);
  result |= ((input & 0x0000FF00) << 8);
  result |= ((input & 0x000000FF) << 24);

  return result;
}

u32 swap_bytes_half(u32 input) {
  u32 result = 0;

  result |= ((input & 0xFFFF0000) >> 16);
  result |= ((input & 0x0000FFFF) << 16);

  return result;
}

// Generates 32-bit value that can be written to SICOMCSR to start a transfer on
// a given channel
u32 generate_com_csr(u32 channel, u32 in_len, u32 out_len) {
  u32 com_csr = 0;

  in_len &= 0x7F;
  out_len &= 0x7F;
  channel &= 0x3;

  // Channel
  com_csr |= (channel << 25);

  // Channel Enable
  com_csr |= (1 << 24);

  // Output Length
  com_csr |= (out_len << 16);

  // Input Length
  com_csr |= (in_len << 8);

  // Command Enable
  com_csr |= (1 << 7);

  // Channel 2?
  com_csr |= (channel << 1);

  // Callback Enable
  com_csr |= (0 << 6);

  // TSTART
  com_csr |= 1;

  si_reg[14] = 0x20202020;

  return com_csr;
}

// Sends the JOYBUS command 0xff to the given channel
// Used to reset and find out the device ID (written to SI buffer)
void reset(u32 channel) {
  // Clear SI buffer
  for (int x = 0; x < 0x20; x++) {
    si_buf[x] = 0;
  }

  // Setup JOYBUS command 0xff
  si_buf[0] = 0xff000000;

  // Write to SICOMCSR to start transfer, wait for any pending transfers first
  while (si_reg[13] & 0x1) {
  }
  si_reg[13] = generate_com_csr(channel, 3, 1);

  // Wait for next SICOMCSR transfer to finish
  while (si_reg[13] & 0x1) {
  }
}

// Sends the JOYBUS command 0x00 to the given channel
// Used to find out the device ID (written to SI buffer)
void ping_ID(u32 channel) {
  // Clear SI buffer
  for (int x = 0; x < 0x20; x++) {
    si_buf[x] = 0;
  }

  // Setup JOYBUS command 0xff
  si_buf[0] = 0x00000000;

  // Write to SICOMCSR to start transfer, wait for any pending transfers first
  while (si_reg[13] & 0x1) {
  }
  si_reg[13] = generate_com_csr(channel, 3, 1);

  // Wait for next SICOMCSR transfer to finish
  while (si_reg[13] & 0x1) {
  }
}

// Sends 0x14 command to GBA
void send_14_cmd(u32 channel) {
  // Clear SI buffer
  for (int x = 0; x < 0x80; x++) {
    si_buf[x] = 0;
  }

  // Set up JOYBUS command
  si_buf[0] = 0x14000000;

  // Clear SISR Error Bits
  si_reg[14] |= (0x0F << ((3 - channel) * 8));

  // Write to SICOMCSR to start transfer, wait for any pending transfers first
  while (si_reg[13] & 0x1) {
  }
  si_reg[13] = generate_com_csr(channel, 4, 1);

  // Wait for next SICOMCSR transfer to finish
  while (si_reg[13] & 0x1) {
  }
}

// Sends 0x15 command to GBA
void send_15_cmd(u32 channel, u8 msB, u8 byte2, u8 byte3, u8 lsB) {
  // Clear SI buffer
  for (int x = 0; x < 0x80; x++) {
    si_buf[x] = 0;
  }

  // Set up JOYBUS command
  si_buf[0] = (0x15 << 24) | (lsB << 16) | (byte3 << 8) | (byte2 << 0);
  si_buf[1] = msB << 24;

  // Clear SISR Error Bits
  si_reg[14] |= (0x0F << ((3 - channel) * 8));

  // Write to SICOMCSR to start transfer, wait for any pending transfers first
  while (si_reg[13] & 0x1) {
  }
  si_reg[13] = generate_com_csr(channel, 1, 5);

  // Wait for next SICOMCSR transfer to finish
  while (si_reg[13] & 0x1) {
  }
}

int main(int argc, char** argv) {
  // Initialise the video system
  VIDEO_Init();

  // This function initialises the attached controllers
  WPAD_Init();

  // Obtain the preferred video mode from the system
  // This will correspond to the settings in the Wii menu
  rmode = VIDEO_GetPreferredMode(NULL);

  // Allocate memory for the display in the uncached region
  xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

  // Initialise the console, required for printf
  console_init(xfb, 20, 20, rmode->fbWidth, rmode->xfbHeight,
               rmode->fbWidth * VI_DISPLAY_PIX_SZ);

  // Set up the video registers with the chosen mode
  VIDEO_Configure(rmode);

  // Tell the video hardware where our display memory is
  VIDEO_SetNextFramebuffer(xfb);

  // Make the display visible
  VIDEO_SetBlack(FALSE);

  // Flush the video register changes to the hardware
  VIDEO_Flush();

  // Wait for Video setup to complete
  VIDEO_WaitVSync();
  if (rmode->viTVMode & VI_NON_INTERLACE)
    VIDEO_WaitVSync();

  // Init FAT
  fatInitDefault();

  u32 vcount = 0;
  bool connected = false;
  u32 counter = 0;

  printf("Insert GBA...\n");

  while (1) {
    vcount++;

    if (vcount >= 30) {
      vcount = 0;

      // Detect Game Boy Advance
      if (!connected) {
        reset(0);
        u32 joybus_id = (si_buf[0] >> 16);

        if (joybus_id == 0x0004) {
          connected = true;
          printf("GBA detected!\n");
          printf("+ = Send, then receive\n- = +1024\n\n");
        }
      } else {
        ping_ID(0);
        u32 joybus_id = (si_buf[0] >> 16);

        if (joybus_id != 0x0004) {
          connected = false;
          printf("GBA disconnected!\nInsert GBA...\n");
        }
      }

      // Call WPAD_ScanPads each loop, this reads the latest controller states
      WPAD_ScanPads();

      // WPAD_ButtonsDown tells us which buttons were pressed in this loop
      // this is a "one shot" state which will not fire again until the button
      // has been released
      u32 pressed = WPAD_ButtonsDown(0);

      if (connected) {
        // Send
        if ((pressed & WPAD_BUTTON_MINUS) || (pressed & WPAD_BUTTON_PLUS)) {
          if (pressed & WPAD_BUTTON_MINUS)
            counter += 1024;
          else
            counter++;

          // Send
          printf(">> %d\n", counter);
          send_15_cmd(0, (counter >> 24) & 0xff, (counter >> 16) & 0xff,
                      (counter >> 8) & 0xff, (counter >> 0) & 0xff);

          // Receive
          send_14_cmd(0);
          u32 value = swap_bytes(si_buf[0]);
          printf("<< %d\n", value);
        }
      }

      // Wait for the next frame
      VIDEO_WaitVSync();
    }
  }

  return 0;
}
