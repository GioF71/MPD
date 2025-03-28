// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "FlacEncoderPlugin.hxx"
#include "../EncoderAPI.hxx"
#include "tag/Names.hxx"
#include "pcm/AudioFormat.hxx"
#include "pcm/Buffer.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "util/DynamicFifoBuffer.hxx"
#include "util/Serial.hxx"
#include "util/SpanCast.hxx"
#include "util/StringUtil.hxx"

#include <FLAC/stream_encoder.h>
#include <FLAC/metadata.h>

#include <algorithm>
#include <utility> // for std::unreachable()

class FlacEncoder final : public Encoder {
	const AudioFormat audio_format;

	FLAC__StreamEncoder *const fse;
	const unsigned compression;
	const bool oggflac;

	PcmBuffer expand_buffer;

	/**
	 * This buffer will hold encoded data from libFLAC until it is
	 * picked up with Read().
	 */
	DynamicFifoBuffer<std::byte> output_buffer{8192};

public:
	FlacEncoder(AudioFormat _audio_format, FLAC__StreamEncoder *_fse, unsigned _compression, bool _oggflac, bool _oggchaining);

	~FlacEncoder() noexcept override {
		FLAC__stream_encoder_delete(fse);
	}

	FlacEncoder(const FlacEncoder &) = delete;
	FlacEncoder &operator=(const FlacEncoder &) = delete;

	/* virtual methods from class Encoder */
	void End() override {
		(void) FLAC__stream_encoder_finish(fse);
	}

	void Flush() override {
	}

	void PreTag() override {
		(void) FLAC__stream_encoder_finish(fse);
	}

	void SendTag(const Tag &tag) override;

	void Write(std::span<const std::byte> src) override;

	std::span<const std::byte> Read(std::span<std::byte>) noexcept override {
		auto r = output_buffer.Read();
		output_buffer.Consume(r.size());
		return r;
	}

private:
	static FLAC__StreamEncoderWriteStatus WriteCallback(const FLAC__StreamEncoder *,
							    const FLAC__byte data[],
							    size_t bytes,
							    [[maybe_unused]] unsigned samples,
							    [[maybe_unused]] unsigned current_frame,
							    void *client_data) noexcept {
		auto &encoder = *(FlacEncoder *)client_data;
		encoder.output_buffer.Append({(const std::byte *)data, bytes});
		return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
	}
};

class PreparedFlacEncoder final : public PreparedEncoder {
	const unsigned compression;
	const bool oggchaining;
	const bool oggflac;

public:
	explicit PreparedFlacEncoder(const ConfigBlock &block);

	/* virtual methods from class PreparedEncoder */
	Encoder *Open(AudioFormat &audio_format) override;

	[[nodiscard]] const char *GetMimeType() const noexcept override {
		if(oggflac)
			return  "audio/ogg";
		return "audio/flac";
	}
};

PreparedFlacEncoder::PreparedFlacEncoder(const ConfigBlock &block)
	:compression(block.GetBlockValue("compression", 5U)),
	oggchaining(block.GetBlockValue("oggchaining",false)),
	oggflac(block.GetBlockValue("oggflac",false) || oggchaining)
{
}

static PreparedEncoder *
flac_encoder_init(const ConfigBlock &block)
{
	return new PreparedFlacEncoder(block);
}

static void
flac_encoder_setup(FLAC__StreamEncoder *fse, unsigned compression, bool oggflac,
		   const AudioFormat &audio_format)
{
	unsigned bits_per_sample;

	switch (audio_format.format) {
	case SampleFormat::S8:
		bits_per_sample = 8;
		break;

	case SampleFormat::S16:
		bits_per_sample = 16;
		break;

	default:
		bits_per_sample = 24;
	}

	if (!FLAC__stream_encoder_set_compression_level(fse, compression))
		throw FmtRuntimeError("error setting flac compression to {}",
				      compression);

	if (!FLAC__stream_encoder_set_channels(fse, audio_format.channels))
		throw FmtRuntimeError("error setting flac channels num to {}",
				      audio_format.channels);

	if (!FLAC__stream_encoder_set_bits_per_sample(fse, bits_per_sample))
		throw FmtRuntimeError("error setting flac bit format to {}",
				      bits_per_sample);

	if (!FLAC__stream_encoder_set_sample_rate(fse,
						  audio_format.sample_rate))
		throw FmtRuntimeError("error setting flac sample rate to {}",
				      audio_format.sample_rate);

	if (oggflac && !FLAC__stream_encoder_set_ogg_serial_number(fse,
						  GenerateSerial()))
		throw std::runtime_error{"error setting ogg serial number"};
}

FlacEncoder::FlacEncoder(AudioFormat _audio_format, FLAC__StreamEncoder *_fse, unsigned _compression, bool _oggflac, bool _oggchaining)
	:Encoder(_oggchaining),
	 audio_format(_audio_format), fse(_fse),
	 compression(_compression),
	 oggflac(_oggflac)
{
	/* this immediately outputs data through callback */

	auto init_status = oggflac ?
		FLAC__stream_encoder_init_ogg_stream(fse,
						     nullptr, WriteCallback,
						     nullptr, nullptr, nullptr,
						     this)
		:
		FLAC__stream_encoder_init_stream(fse,
						 WriteCallback,
						 nullptr, nullptr, nullptr,
						 this);

	if (init_status != FLAC__STREAM_ENCODER_INIT_STATUS_OK)
		throw FmtRuntimeError("failed to initialize encoder: {}",
				      FLAC__StreamEncoderInitStatusString[init_status]);
}

Encoder *
PreparedFlacEncoder::Open(AudioFormat &audio_format)
{
	switch (audio_format.format) {
	case SampleFormat::S8:
		break;

	case SampleFormat::S16:
		break;

	case SampleFormat::S24_P32:
		break;

	default:
		audio_format.format = SampleFormat::S24_P32;
	}

	/* allocate the encoder */
	auto fse = FLAC__stream_encoder_new();
	if (fse == nullptr)
		throw std::runtime_error("FLAC__stream_encoder_new() failed");

	try {
		flac_encoder_setup(fse, compression, oggflac, audio_format);
	} catch (...) {
		FLAC__stream_encoder_delete(fse);
		throw;
	}

	return new FlacEncoder(audio_format, fse, compression, oggflac, oggchaining);
}

void
FlacEncoder::SendTag(const Tag &tag)
{
	/* re-initialize encoder since flac_encoder_finish resets everything */
	flac_encoder_setup(fse, compression, oggflac, audio_format);

	FLAC__StreamMetadata *metadata = FLAC__metadata_object_new(FLAC__METADATA_TYPE_VORBIS_COMMENT);
	FLAC__StreamMetadata_VorbisComment_Entry entry;

	for (const auto &item : tag) {
		char name[64];
		ToUpperASCII(name, tag_item_names[item.type], sizeof(name));
		FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&entry, name, item.value);
		FLAC__metadata_object_vorbiscomment_append_comment(metadata, entry, false);
	}

	FLAC__stream_encoder_set_metadata(fse,&metadata,1);

	auto init_status = FLAC__stream_encoder_init_ogg_stream(fse,
							        nullptr, WriteCallback,
							        nullptr, nullptr, nullptr,
							        this);

	FLAC__metadata_object_delete(metadata);

	if (init_status != FLAC__STREAM_ENCODER_INIT_STATUS_OK)
		throw FmtRuntimeError("failed to initialize encoder: {}",
				      FLAC__StreamEncoderInitStatusString[init_status]);
}

template<typename T>
static std::span<const FLAC__int32>
ToFlac32(PcmBuffer &buffer, std::span<const T> src) noexcept
{
	FLAC__int32 *dest = buffer.GetT<FLAC__int32>(src.size());
	std::copy(src.begin(), src.end(), dest);
	return {dest, src.size()};
}

static std::span<const FLAC__int32>
ToFlac32(PcmBuffer &buffer, std::span<const std::byte> src,
	 SampleFormat format)
{
	switch (format) {
	case SampleFormat::S8:
		return ToFlac32(buffer, FromBytesStrict<const int8_t>(src));

	case SampleFormat::S16:
		return ToFlac32(buffer, FromBytesStrict<const int16_t>(src));

	case SampleFormat::S24_P32:
	case SampleFormat::S32:
		/* nothing need to be done; format is the same for
		   both mpd and libFLAC */
		return FromBytesStrict<const int32_t>(src);

	default:
		std::unreachable();
	}
}

void
FlacEncoder::Write(std::span<const std::byte> src)
{
	const auto imported = ToFlac32(expand_buffer, src,
				       audio_format.format);
	const std::size_t n_frames = imported.size() / audio_format.channels;

	/* feed samples to encoder */

	if (!FLAC__stream_encoder_process_interleaved(fse, imported.data(),
						      n_frames))
		throw std::runtime_error("flac encoder process failed");
}

const EncoderPlugin flac_encoder_plugin = {
	"flac",
	flac_encoder_init,
};

