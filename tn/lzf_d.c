#include <common.h>

u32 lzf_decompress(const void *in_data, u32 in_len, void *out_data, u32 out_len)
{
	const u8 *ip = (const u8 *)in_data;
	u8 *op = (u8 *)out_data;
	const u8 *in_end = ip + in_len;
	const u8 *out_end = op + out_len;

	do
	{
		u32 ctrl = *ip++;

		if(ctrl < 32) /* literal run */
		{
			ctrl++;

			if((op + ctrl) > out_end) return 0;
			if((ip + ctrl) > in_end) return 0;

			int i;
			for(i = ctrl; i > 0; i--)
			{
				*op++ = *ip++;
			}
		}
		else /* back reference */
		{
			u32 len = ctrl >> 5;

			u8 *ref = op - ((ctrl & 0x1F) << 8) - 1;

			if(ip >= in_end) return 0;

			if(len == 7)
			{
				len += *ip++;
				if(ip >= in_end) return 0;
			}

			ref -= *ip++;

			if((op + len + 2) > out_end) return 0;
			if(ref < (u8 *)out_data) return 0;

			if(len > 9)
			{
				len += 2;

				if(op >= (ref + len))
				{
					_memcpy(op, ref, len);
					op += len;
				}
				else
				{
					do
					{
						*op++ = *ref++;
					} while(--len);
				}
			}
			else
			{
				int i;
				for(i = len; i >= 0; i--)
				{
					*op++ = *ref++;
				}

				*op++ = *ref++;
			}
		}
	} while(ip < in_end);

	return op - (u8 *)out_data;
}