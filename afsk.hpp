#pragma once
#include <string>
#include <vector>

#include "utils.hpp"

struct AFSK
{
	static const int baud_rate = 1200;			// adjust sample_rate accordingly when changing this
	static const size_t sample_rate = 8 * baud_rate;	// easier to demod

	class Encoder
	{
		struct synth_state
		{
			// current frequency
			uintmax_t freq = 1200;

			// internal bit counter - needed for proper timing
			uintmax_t total_bits = 0;

			// current LUT index
			uintmax_t idx = 0;
		};

	public:
		// Encodes an AFSK NRZI message
		static std::vector<uint8_t> Encode(
			const std::vector<uint8_t> &message,
			int begin_marker_size = 1,
			int end_marker_size = 1)
		{
			const int samples_required = sample_rate / baud_rate * message.size();

			std::vector<uint8_t> result;
			result.reserve(1 + begin_marker_size + samples_required + end_marker_size + 1);

			synth_state state;

			// write several 0x7E's to allow the receiver to synchronize
			synth(std::vector<uint8_t>(begin_marker_size / 2, 0x00), false, state, result);
			synth(std::vector<uint8_t>(end_marker_size - begin_marker_size / 2, 0x7E), false, state, result);
			synth(message, true, state, result);

			// and more 0x7E's for good measure
			synth(std::vector<uint8_t>(end_marker_size - end_marker_size / 2, 0x7E), false, state, result);
			synth(std::vector<uint8_t>(end_marker_size / 2, 0x00), false, state, result);

			return result;
		}

	private:
		static void synth(
			const std::vector<uint8_t> &message,
			const bool escape,
			synth_state &state,
			std::vector<uint8_t> &output)
		{
			// frequency step - a minimum change in signal frequency that is for current sample rate
			// this is actually reduced to 1 Hz when calculating samples
			const int freq_step = sample_rate / utils::lut_size;

			// baud step - a number of samples comprising 1 baud
			const int baud_step = sample_rate / baud_rate;

			int ones = 0;
			float mul = 1;

			for (uint16_t byte : message)
			{
				for (int bit = 0; bit < 8; ++bit)
				{
					++state.total_bits;

					if (escape && ++ones == 6)
					{
						ones = 0;
						byte <<= 1;								// holy fuck-knuckles
						--bit;
					}

					// NRZI encoding
					if ((byte & 0x01) == 0)
					{
						// wiggle frequency if bit is unset
						wiggle(state.freq, mul);
						ones = 0;
					}

					byte >>= 1;

					// write samples
					while (output.size() < state.total_bits * baud_step)
					{
						// do a LUT lookup
						uint8_t x = 0.5 * utils::lut[(state.idx / freq_step) % utils::lut_size];

						// write sample
						output.push_back(x);

						// advance in LUT
						state.idx += state.freq;
					}
				}
			}
		}

		static void wiggle(uintmax_t &freq, float &mul)
		{
			constexpr uintmax_t freq_hi = 2200;
			constexpr uintmax_t freq_lo = 1200;

			if (freq == freq_lo)
			{
				freq = freq_hi;
				mul = 0.8f;
			}
			else
			{
				freq = freq_lo;
				mul = 0.8f;
			}
		}
	};

	class Decoder
	{
	public:
		// slow but easier to read
		static std::vector<uint8_t> demod_naive(
			const std::vector<uint8_t> &samples,
			size_t test_shift)
		{
			// determine the frequencies of each baud
			std::vector<int> frequencies;

			for (size_t i = test_shift; i < samples.size() - 8 - test_shift; i += 8)
			{
				frequencies.push_back(fft2(samples, i) > 0);
			}

			// decode the frequencies from NZRI
			std::vector<int> nzri_bits;

			for (size_t i = 1; i < frequencies.size(); ++i)
			{
				nzri_bits.push_back(frequencies[i] == frequencies[i - 1]);
			}

			uint8_t byte_buf = 0;

			int start = 0;
			bool decoding = false;

			std::vector<uint8_t> buf;

			int ones = 0;

			int escape = 0;

			// finally, decode the bitstream into bytes
			for (size_t i = 0; i < nzri_bits.size(); ++i)
			{
				// bits are received backwards
				byte_buf >>= 1;
				byte_buf |= (nzri_bits[i] << 7);

				// check whether we should skip a bit
				if (!escape)
				{
					// can we synchronize?
					if (byte_buf == 0x7E)
					{
						// is this an end marker?
						if (decoding && !buf.empty())
						{
							return buf;
						}

						// otherwise, synchronize and start decoding
						decoding = true;
						byte_buf = 0;
						start = i;
						continue;
					}
				}
				else
				{
					// skip a bit
					--escape;
				}

				// skip bits until we can synchronize (last 8 bits == 0x7E)
				if (!decoding)
				{
					continue;
				}

				if (nzri_bits[i])
				{
					++ones;
				}
				else
				{
					if (ones == 5)
					{
						byte_buf <<= 1;
						++start;
						ones = 0;
						escape = 2;
						continue;
					}

					ones = 0;
				}

				if (((i - start) % 8) == 0)
				{
					buf.push_back(byte_buf);
					byte_buf = 0;
				}
			}

			return buf;
		}

		struct demod_state
		{
			enum iter_result
			{
				DEMOD_START_STOP = 1,
				DEMOD_BIT_READ,
				DEMOD_BIT_SKIP,
			};

			bool last_freq = 0;
			uint8_t byte_buf = 0;
			int escape = 0;
			int start = 0;
			int bit = 0;
			int ones = 0;
			bool decoding = 0;
		};

		// optimized
		static int demod_iter(demod_state &state, const std::vector<uint8_t> &samples, size_t i)
		{
			++state.bit;

			bool freq = fft2(samples, i) > 0;
			bool is_set = state.last_freq == freq;
			state.last_freq = freq;

			state.byte_buf >>= 1;
			state.byte_buf |= (is_set << 7);

			if (!state.escape)
			{
				if (state.byte_buf == 0x7E)
				{
					state.decoding = true;
					state.byte_buf = 0;
					state.start = state.bit;
					return -demod_state::DEMOD_START_STOP;
				}
			}
			else
			{
				--state.escape;
			}

			if (!state.decoding)
			{
				return -demod_state::DEMOD_BIT_SKIP;
			}

			if (is_set)
			{
				++state.ones;
			}
			else
			{
				if (state.ones == 5)
				{
					state.byte_buf <<= 1;
					++state.start;
					state.ones = 0;
					state.escape = 2;
					return -demod_state::DEMOD_BIT_SKIP;
				}

				state.ones = 0;
			}

			if (((state.bit - state.start) & 0b0111) != 0)
			{
				return -demod_state::DEMOD_BIT_SKIP;
			}

			auto result = state.byte_buf;
			state.byte_buf = 0;
			return result;
		}

		static std::vector<uint8_t> demod(
			const std::vector<uint8_t> &samples,
			size_t test_shift)
		{
			demod_state state;
			std::vector<uint8_t> buf;
			buf.reserve(512);

			for (size_t i = test_shift, bit = 0; i < samples.size() - 8 - test_shift; i += 8, ++bit)
			{
				auto result = demod_iter(state, samples, i);

				switch (result)
				{
					case -demod_state::DEMOD_START_STOP:

						if (buf.size() > 15)
						{
							return buf;
						}
						else
						{
							buf.clear();
						}

						break;

					case -demod_state::DEMOD_BIT_SKIP:
						break;

					default:
						buf.push_back(result);
				}
			}

			return { };
		}

	private:
		static int fft2(std::vector<uint8_t> data, int idx)
		{
			int8_t coeffloi[] = {64, 45, 0, -45, -64, -45, 0, 45};
			int8_t coeffloq[] = {0, 45, 64, 45, 0, -45, -64, -45};
			int8_t coeffhii[] = {64, 8, -62, -24, 55, 39, -45, -51};
			int8_t coeffhiq[] = {0, 63, 17, -59, -32, 51, 45, -39};

			int outloi = 0, outloq = 0, outhii = 0, outhiq = 0;

			for (int ii = 0; ii < 8; ii++)
			{
				int sample = data[ii + idx] - 128;
				outloi += sample * coeffloi[ii];
				outloq += sample * coeffloq[ii];
				outhii += sample * coeffhii[ii];
				outhiq += sample * coeffhiq[ii];
			}

			return (outhii >> 8) * (outhii >> 8) + (outhiq >> 8) * (outhiq >> 8) -
			       (outloi >> 8) * (outloi >> 8) - (outloq >> 8) * (outloq >> 8);
		}
	};
};
