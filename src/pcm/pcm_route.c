/**
 * \file pcm/pcm_route.c
 * \ingroup PCM_Plugins
 * \brief PCM Route & Volume Plugin Interface
 * \author Abramo Bagnara <abramo@alsa-project.org>
 * \date 2000-2001
 */
/*
 *  PCM - Route & Volume Plugin
 *  Copyright (c) 2000 by Abramo Bagnara <abramo@alsa-project.org>
 *
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as
 *   published by the Free Software Foundation; either version 2.1 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
  
#include "bswap.h"
#include <math.h>
#include "pcm_local.h"
#include "pcm_plugin.h"

#include "plugin_ops.h"

#ifndef PIC
/* entry for static linking */
const char *_snd_module_pcm_route = "";
#endif

#ifndef DOC_HIDDEN

/* The best possible hack to support missing optimization in gcc 2.7.2.3 */
#if SND_PCM_PLUGIN_ROUTE_RESOLUTION & (SND_PCM_PLUGIN_ROUTE_RESOLUTION - 1) != 0
#define div(a) a /= SND_PCM_PLUGIN_ROUTE_RESOLUTION
#elif SND_PCM_PLUGIN_ROUTE_RESOLUTION == 16
#define div(a) a >>= 4
#else
#error "Add some code here"
#endif

typedef struct {
	int channel;
	int as_int;
#if SND_PCM_PLUGIN_ROUTE_FLOAT
	float as_float;
#endif
} snd_pcm_route_ttable_src_t;

typedef struct snd_pcm_route_ttable_dst snd_pcm_route_ttable_dst_t;

typedef struct {
	enum {UINT64, FLOAT} sum_idx;
	unsigned int get_idx;
	unsigned int put_idx;
	unsigned int conv_idx;
	int use_getput;
	unsigned int src_size;
	snd_pcm_format_t dst_sfmt;
	unsigned int nsrcs;
	unsigned int ndsts;
	snd_pcm_route_ttable_dst_t *dsts;
} snd_pcm_route_params_t;


typedef void (*route_f)(const snd_pcm_channel_area_t *dst_area,
			snd_pcm_uframes_t dst_offset,
			const snd_pcm_channel_area_t *src_areas,
			snd_pcm_uframes_t src_offset,
			unsigned int src_channels,
			snd_pcm_uframes_t frames,
			const snd_pcm_route_ttable_dst_t *ttable,
			const snd_pcm_route_params_t *params);

struct snd_pcm_route_ttable_dst {
	int att;	/* Attenuated */
	unsigned int nsrcs;
	snd_pcm_route_ttable_src_t* srcs;
	route_f func;
};

typedef union {
	int32_t as_sint32;
	int64_t as_sint64;
#if SND_PCM_PLUGIN_ROUTE_FLOAT
	float as_float;
#endif
} sum_t;

typedef struct {
	/* This field need to be the first */
	snd_pcm_plugin_t plug;
	snd_pcm_format_t sformat;
	int schannels;
	snd_pcm_route_params_t params;
	snd_pcm_chmap_t *chmap;
} snd_pcm_route_t;

#endif /* DOC_HIDDEN */

static void snd_pcm_route_convert1_zero(const snd_pcm_channel_area_t *dst_area,
					snd_pcm_uframes_t dst_offset,
					const snd_pcm_channel_area_t *src_areas ATTRIBUTE_UNUSED,
					snd_pcm_uframes_t src_offset ATTRIBUTE_UNUSED,
					unsigned int src_channels ATTRIBUTE_UNUSED,
					snd_pcm_uframes_t frames,
					const snd_pcm_route_ttable_dst_t* ttable ATTRIBUTE_UNUSED,
					const snd_pcm_route_params_t *params)
{
	snd_pcm_area_silence(dst_area, dst_offset, frames, params->dst_sfmt);
}

#ifndef DOC_HIDDEN

static void snd_pcm_route_convert1_one(const snd_pcm_channel_area_t *dst_area,
				       snd_pcm_uframes_t dst_offset,
				       const snd_pcm_channel_area_t *src_areas,
				       snd_pcm_uframes_t src_offset,
				       unsigned int src_channels,
				       snd_pcm_uframes_t frames,
				       const snd_pcm_route_ttable_dst_t* ttable,
				       const snd_pcm_route_params_t *params)
{
	const snd_pcm_channel_area_t *src_area = 0;
	unsigned int srcidx;
	const char *src;
	char *dst;
	int src_step, dst_step;
	for (srcidx = 0; srcidx < ttable->nsrcs && srcidx < src_channels; ++srcidx) {
		unsigned int channel = ttable->srcs[srcidx].channel;
		if (channel >= src_channels)
			continue;
		src_area = &src_areas[channel];
		if (src_area->addr != NULL)
			break;
	}
	if (srcidx == ttable->nsrcs || srcidx == src_channels) {
		snd_pcm_route_convert1_zero(dst_area, dst_offset,
					    src_areas, src_offset,
					    src_channels,
					    frames, ttable, params);
		return;
	}
	
	src = snd_pcm_channel_area_addr(src_area, src_offset);
	dst = snd_pcm_channel_area_addr(dst_area, dst_offset);
	src_step = snd_pcm_channel_area_step(src_area);
	dst_step = snd_pcm_channel_area_step(dst_area);
	while (frames-- > 0) {
		conv(dst, src, params->conv_idx);
		src += src_step;
		dst += dst_step;
	}
}

static void snd_pcm_route_convert1_one_getput(const snd_pcm_channel_area_t *dst_area,
					      snd_pcm_uframes_t dst_offset,
					      const snd_pcm_channel_area_t *src_areas,
					      snd_pcm_uframes_t src_offset,
					      unsigned int src_channels,
					      snd_pcm_uframes_t frames,
					      const snd_pcm_route_ttable_dst_t* ttable,
					      const snd_pcm_route_params_t *params)
{
	const snd_pcm_channel_area_t *src_area = 0;
	unsigned int srcidx;
	const char *src;
	char *dst;
	int src_step, dst_step;
	uint32_t sample = 0;
	for (srcidx = 0; srcidx < ttable->nsrcs && srcidx < src_channels; ++srcidx) {
		unsigned int channel = ttable->srcs[srcidx].channel;
		if (channel >= src_channels)
			continue;
		src_area = &src_areas[channel];
		if (src_area->addr != NULL)
			break;
	}
	if (srcidx == ttable->nsrcs || srcidx == src_channels) {
		snd_pcm_route_convert1_zero(dst_area, dst_offset,
					    src_areas, src_offset,
					    src_channels,
					    frames, ttable, params);
		return;
	}
	
	src = snd_pcm_channel_area_addr(src_area, src_offset);
	dst = snd_pcm_channel_area_addr(dst_area, dst_offset);
	src_step = snd_pcm_channel_area_step(src_area);
	dst_step = snd_pcm_channel_area_step(dst_area);
	while (frames-- > 0) {
		sample = get32(src, params->get_idx);
		put32(dst, sample, params->put_idx);
		src += src_step;
		dst += dst_step;
	}
}

static void snd_pcm_route_convert1_many(const snd_pcm_channel_area_t *dst_area,
					snd_pcm_uframes_t dst_offset,
					const snd_pcm_channel_area_t *src_areas,
					snd_pcm_uframes_t src_offset,
					unsigned int src_channels,
					snd_pcm_uframes_t frames,
					const snd_pcm_route_ttable_dst_t* ttable,
					const snd_pcm_route_params_t *params)
{
	int nsrcs = ttable->nsrcs;
	char *dst;
	int dst_step;
	const char *srcs[nsrcs];
	int src_steps[nsrcs];
	snd_pcm_route_ttable_src_t src_tt[nsrcs];
	int32_t sample = 0;
	int srcidx, srcidx1 = 0;
	for (srcidx = 0; srcidx < nsrcs && (unsigned)srcidx < src_channels; ++srcidx) {
		const snd_pcm_channel_area_t *src_area;
		unsigned int channel = ttable->srcs[srcidx].channel;
		if (channel >= src_channels)
			continue;
		src_area = &src_areas[channel];
		srcs[srcidx1] = snd_pcm_channel_area_addr(src_area, src_offset);
		src_steps[srcidx1] = snd_pcm_channel_area_step(src_area);
		src_tt[srcidx1] = ttable->srcs[srcidx];
		srcidx1++;
	}
	nsrcs = srcidx1;
	if (nsrcs == 0) {
		snd_pcm_route_convert1_zero(dst_area, dst_offset,
					    src_areas, src_offset,
					    src_channels,
					    frames, ttable, params);
		return;
	} else if (nsrcs == 1 && src_tt[0].as_int == SND_PCM_PLUGIN_ROUTE_RESOLUTION) {
		if (params->use_getput)
			snd_pcm_route_convert1_one_getput(dst_area, dst_offset,
							  src_areas, src_offset,
							  src_channels,
							  frames, ttable, params);
		else
			snd_pcm_route_convert1_one(dst_area, dst_offset,
						   src_areas, src_offset,
						   src_channels,
						   frames, ttable, params);
		return;
	}

	dst = snd_pcm_channel_area_addr(dst_area, dst_offset);
	dst_step = snd_pcm_channel_area_step(dst_area);

	while (frames-- > 0) {
		snd_pcm_route_ttable_src_t *ttp = src_tt;
		sum_t sum;

		/* Zero sum */
		switch (params->sum_idx) {
		case 0: sum.as_sint64 = 0; break;
#if SND_PCM_PLUGIN_ROUTE_FLOAT
		case 1: sum.as_float = 0.0; break;
#endif
		}
		for (srcidx = 0; srcidx < nsrcs; ++srcidx) {
			const char *src = srcs[srcidx];
			
			/* Get sample */
			sample = get32(src, params->get_idx);

			/* Sum */
			switch (params->sum_idx * 2 + ttable->att) {
			case 0:
				if (ttp->as_int)
					sum.as_sint64 += sample;
				break;
			case 1:
				sum.as_sint64 += (int64_t) sample * ttp->as_int;
				break;
#if SND_PCM_PLUGIN_ROUTE_FLOAT
			case 2:
				if (ttp->as_int)
					sum.as_float += sample;
				break;
			case 3:
				sum.as_float += sample * ttp->as_float;
				break;
#endif
			}
			srcs[srcidx] += src_steps[srcidx];
			ttp++;
		}
		
		/* Normalization */
		switch (params->sum_idx * 2 + ttable->att) {
		case 1:
			div(sum.as_sint64);
			/* fallthru */
		case 0:
			if (sum.as_sint64 > (int64_t)0x7fffffff)
				sample = 0x7fffffff;	/* maximum positive value */
			else if (sum.as_sint64 < -(int64_t)0x80000000)
				sample = 0x80000000;	/* maximum negative value */
			else
				sample = sum.as_sint64;
			break;
#if SND_PCM_PLUGIN_ROUTE_FLOAT
		case 2:
		case 3:
			sum.as_float = rint(sum.as_float);
			if (sum.as_float > (int64_t)0x7fffffff)
				sample = 0x7fffffff;	/* maximum positive value */
			else if (sum.as_float < -(int64_t)0x80000000)
				sample = 0x80000000;	/* maximum negative value */
			else
				sample = sum.as_float;
			break;
#endif
		}
		
		/* Put sample */
		put32(dst, sample, params->put_idx);

		dst += dst_step;
	}
}

#endif /* DOC_HIDDEN */

static void snd_pcm_route_convert(const snd_pcm_channel_area_t *dst_areas,
				  snd_pcm_uframes_t dst_offset,
				  const snd_pcm_channel_area_t *src_areas,
				  snd_pcm_uframes_t src_offset,
				  unsigned int src_channels,
				  unsigned int dst_channels,
				  snd_pcm_uframes_t frames,
				  snd_pcm_route_params_t *params)
{
	unsigned int dst_channel;
	snd_pcm_route_ttable_dst_t *dstp;
	const snd_pcm_channel_area_t *dst_area;

	dstp = params->dsts;
	dst_area = dst_areas;
	for (dst_channel = 0; dst_channel < dst_channels; ++dst_channel) {
		if (dst_channel >= params->ndsts)
			snd_pcm_route_convert1_zero(dst_area, dst_offset,
						    src_areas, src_offset,
						    src_channels,
						    frames, dstp, params);
		else
			dstp->func(dst_area, dst_offset,
				   src_areas, src_offset,
				   src_channels,
				   frames, dstp, params);
		dstp++;
		dst_area++;
	}
}

static int snd_pcm_route_close(snd_pcm_t *pcm)
{
	snd_pcm_route_t *route = pcm->private_data;
	snd_pcm_route_params_t *params = &route->params;
	unsigned int dst_channel;

	if (params->dsts) {
		for (dst_channel = 0; dst_channel < params->ndsts; ++dst_channel) {
			free(params->dsts[dst_channel].srcs);
		}
		free(params->dsts);
	}
	free(route->chmap);
	return snd_pcm_generic_close(pcm);
}

static int snd_pcm_route_hw_refine_cprepare(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_hw_params_t *params)
{
	int err;
	snd_pcm_access_mask_t access_mask = { SND_PCM_ACCBIT_SHM };
	snd_pcm_format_mask_t format_mask = { SND_PCM_FMTBIT_LINEAR };
	err = _snd_pcm_hw_param_set_mask(params, SND_PCM_HW_PARAM_ACCESS,
					 &access_mask);
	if (err < 0)
		return err;
	err = _snd_pcm_hw_param_set_mask(params, SND_PCM_HW_PARAM_FORMAT,
					 &format_mask);
	if (err < 0)
		return err;
	err = _snd_pcm_hw_params_set_subformat(params, SND_PCM_SUBFORMAT_STD);
	if (err < 0)
		return err;
	err = _snd_pcm_hw_param_set_min(params, SND_PCM_HW_PARAM_CHANNELS, 1, 0);
	if (err < 0)
		return err;
	params->info &= ~(SND_PCM_INFO_MMAP | SND_PCM_INFO_MMAP_VALID);
	return 0;
}

static int snd_pcm_route_hw_refine_sprepare(snd_pcm_t *pcm, snd_pcm_hw_params_t *sparams)
{
	snd_pcm_route_t *route = pcm->private_data;
	snd_pcm_access_mask_t saccess_mask = { SND_PCM_ACCBIT_MMAP };
	_snd_pcm_hw_params_any(sparams);
	_snd_pcm_hw_param_set_mask(sparams, SND_PCM_HW_PARAM_ACCESS,
				   &saccess_mask);
	if (route->sformat != SND_PCM_FORMAT_UNKNOWN) {
		_snd_pcm_hw_params_set_format(sparams, route->sformat);
		_snd_pcm_hw_params_set_subformat(sparams, SND_PCM_SUBFORMAT_STD);
	}
	if (route->schannels >= 0) {
		_snd_pcm_hw_param_set(sparams, SND_PCM_HW_PARAM_CHANNELS,
				      (unsigned int) route->schannels, 0);
	}
	return 0;
}

static int snd_pcm_route_hw_refine_schange(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
					    snd_pcm_hw_params_t *sparams)
{
	snd_pcm_route_t *route = pcm->private_data;
	int err;
	unsigned int links = (SND_PCM_HW_PARBIT_RATE |
			      SND_PCM_HW_PARBIT_PERIODS |
			      SND_PCM_HW_PARBIT_PERIOD_SIZE |
			      SND_PCM_HW_PARBIT_PERIOD_TIME |
			      SND_PCM_HW_PARBIT_BUFFER_SIZE |
			      SND_PCM_HW_PARBIT_BUFFER_TIME |
			      SND_PCM_HW_PARBIT_TICK_TIME);
	if (route->sformat == SND_PCM_FORMAT_UNKNOWN)
		links |= (SND_PCM_HW_PARBIT_FORMAT | 
			  SND_PCM_HW_PARBIT_SUBFORMAT |
			  SND_PCM_HW_PARBIT_SAMPLE_BITS);
	if (route->schannels < 0)
		links |= SND_PCM_HW_PARBIT_CHANNELS;
	err = _snd_pcm_hw_params_refine(sparams, links, params);
	if (err < 0)
		return err;
	return 0;
}
	
static int snd_pcm_route_hw_refine_cchange(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
					    snd_pcm_hw_params_t *sparams)
{
	snd_pcm_route_t *route = pcm->private_data;
	int err;
	unsigned int links = (SND_PCM_HW_PARBIT_RATE |
			      SND_PCM_HW_PARBIT_PERIODS |
			      SND_PCM_HW_PARBIT_PERIOD_SIZE |
			      SND_PCM_HW_PARBIT_PERIOD_TIME |
			      SND_PCM_HW_PARBIT_BUFFER_SIZE |
			      SND_PCM_HW_PARBIT_BUFFER_TIME |
			      SND_PCM_HW_PARBIT_TICK_TIME);
	if (route->sformat == SND_PCM_FORMAT_UNKNOWN)
		links |= (SND_PCM_HW_PARBIT_FORMAT | 
			  SND_PCM_HW_PARBIT_SUBFORMAT |
			  SND_PCM_HW_PARBIT_SAMPLE_BITS);
	if (route->schannels < 0)
		links |= SND_PCM_HW_PARBIT_CHANNELS;
	err = _snd_pcm_hw_params_refine(params, links, sparams);
	if (err < 0)
		return err;
	return 0;
}

static int snd_pcm_route_hw_refine(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	return snd_pcm_hw_refine_slave(pcm, params,
				       snd_pcm_route_hw_refine_cprepare,
				       snd_pcm_route_hw_refine_cchange,
				       snd_pcm_route_hw_refine_sprepare,
				       snd_pcm_route_hw_refine_schange,
				       snd_pcm_generic_hw_refine);
}

static int snd_pcm_route_hw_params(snd_pcm_t *pcm, snd_pcm_hw_params_t * params)
{
	snd_pcm_route_t *route = pcm->private_data;
	snd_pcm_t *slave = route->plug.gen.slave;
	snd_pcm_format_t src_format, dst_format;
	int err = snd_pcm_hw_params_slave(pcm, params,
					  snd_pcm_route_hw_refine_cchange,
					  snd_pcm_route_hw_refine_sprepare,
					  snd_pcm_route_hw_refine_schange,
					  snd_pcm_generic_hw_params);
	if (err < 0)
		return err;

	if (pcm->stream == SND_PCM_STREAM_PLAYBACK) {
		err = INTERNAL(snd_pcm_hw_params_get_format)(params, &src_format);
		dst_format = slave->format;
	} else {
		src_format = slave->format;
		err = INTERNAL(snd_pcm_hw_params_get_format)(params, &dst_format);
	}
	if (err < 0)
		return err;
	/* 3 bytes or 20-bit formats? */
	route->params.use_getput =
		(snd_pcm_format_physical_width(src_format) + 7) / 8 == 3 ||
		(snd_pcm_format_physical_width(dst_format) + 7) / 8 == 3 ||
		snd_pcm_format_width(src_format) == 20 ||
		snd_pcm_format_width(dst_format) == 20;
	route->params.get_idx = snd_pcm_linear_get_index(src_format, SND_PCM_FORMAT_S32);
	route->params.put_idx = snd_pcm_linear_put_index(SND_PCM_FORMAT_S32, dst_format);
	route->params.conv_idx = snd_pcm_linear_convert_index(src_format, dst_format);
	route->params.src_size = snd_pcm_format_width(src_format) / 8;
	route->params.dst_sfmt = dst_format;
#if SND_PCM_PLUGIN_ROUTE_FLOAT
	route->params.sum_idx = FLOAT;
#else
	route->params.sum_idx = UINT64;
#endif
	return 0;
}

static snd_pcm_uframes_t
snd_pcm_route_write_areas(snd_pcm_t *pcm,
			  const snd_pcm_channel_area_t *areas,
			  snd_pcm_uframes_t offset,
			  snd_pcm_uframes_t size,
			  const snd_pcm_channel_area_t *slave_areas,
			  snd_pcm_uframes_t slave_offset,
			  snd_pcm_uframes_t *slave_sizep)
{
	snd_pcm_route_t *route = pcm->private_data;
	snd_pcm_t *slave = route->plug.gen.slave;
	if (size > *slave_sizep)
		size = *slave_sizep;
	snd_pcm_route_convert(slave_areas, slave_offset,
			      areas, offset, 
			      pcm->channels,
			      slave->channels,
			      size, &route->params);
	*slave_sizep = size;
	return size;
}

static snd_pcm_uframes_t
snd_pcm_route_read_areas(snd_pcm_t *pcm,
			 const snd_pcm_channel_area_t *areas,
			 snd_pcm_uframes_t offset,
			 snd_pcm_uframes_t size,
			 const snd_pcm_channel_area_t *slave_areas,
			 snd_pcm_uframes_t slave_offset,
			 snd_pcm_uframes_t *slave_sizep)
{
	snd_pcm_route_t *route = pcm->private_data;
	snd_pcm_t *slave = route->plug.gen.slave;
	if (size > *slave_sizep)
		size = *slave_sizep;
	snd_pcm_route_convert(areas, offset, 
			      slave_areas, slave_offset,
			      slave->channels,
			      pcm->channels,
			      size, &route->params);
	*slave_sizep = size;
	return size;
}

static snd_pcm_chmap_t *snd_pcm_route_get_chmap(snd_pcm_t *pcm)
{
	snd_pcm_route_t *route = pcm->private_data;
	snd_pcm_chmap_t *map, *slave_map;
	unsigned int src, dst, nsrcs;

	slave_map = snd_pcm_generic_get_chmap(pcm);
	if (!slave_map)
		return NULL;
	nsrcs = route->params.nsrcs;
	map = calloc(4, nsrcs + 1);
	if (!map) {
		free(slave_map);
		return NULL;
	}
	map->channels = nsrcs;
	for (src = 0; src < nsrcs; src++)
		map->pos[src] = SND_CHMAP_NA;
	for (dst = 0; dst < route->params.ndsts; dst++) {
		snd_pcm_route_ttable_dst_t *d = &route->params.dsts[dst];
		for (src = 0; src < d->nsrcs; src++) {
			unsigned int c = d->srcs[src].channel;
			if (c < nsrcs && map->pos[c] == SND_CHMAP_NA)
				map->pos[c] = slave_map->pos[dst];
		}
	}
	free(slave_map);
	return map;
}

static snd_pcm_chmap_query_t **snd_pcm_route_query_chmaps(snd_pcm_t *pcm)
{
	snd_pcm_chmap_query_t **maps;
	snd_pcm_chmap_t *map = snd_pcm_route_get_chmap(pcm);
	if (!map)
		return NULL;
	maps = _snd_pcm_make_single_query_chmaps(map);
	free(map);
	return maps;
}

static void snd_pcm_route_dump(snd_pcm_t *pcm, snd_output_t *out)
{
	snd_pcm_route_t *route = pcm->private_data;
	unsigned int dst;
	if (route->sformat == SND_PCM_FORMAT_UNKNOWN)
		snd_output_printf(out, "Route conversion PCM\n");
	else
		snd_output_printf(out, "Route conversion PCM (sformat=%s)\n", 
			snd_pcm_format_name(route->sformat));
	snd_output_puts(out, "  Transformation table:\n");
	for (dst = 0; dst < route->params.ndsts; dst++) {
		snd_pcm_route_ttable_dst_t *d = &route->params.dsts[dst];
		unsigned int src;
		snd_output_printf(out, "    %d <- ", dst);
		if (d->nsrcs == 0) {
			snd_output_printf(out, "none\n");
			continue;
		}
		src = 0;
		while (1) {
			snd_pcm_route_ttable_src_t *s = &d->srcs[src];
			if (d->att)
#if SND_PCM_PLUGIN_ROUTE_FLOAT
				snd_output_printf(out, "%d*%g", s->channel, s->as_float);
#else
				snd_output_printf(out, "%d*%g", s->channel, (double)s->as_int / (double)SND_PCM_PLUGIN_ROUTE_RESOLUTION);
#endif
			else
				snd_output_printf(out, "%d", s->channel);
			src++;
			if (src == d->nsrcs)
				break;
			snd_output_puts(out, " + ");
		}
		snd_output_putc(out, '\n');
	}
	if (pcm->setup) {
		snd_output_printf(out, "Its setup is:\n");
		snd_pcm_dump_setup(pcm, out);
	}
	snd_output_printf(out, "Slave: ");
	snd_pcm_dump(route->plug.gen.slave, out);
}

/*
 * Converts a string to an array of channel indices:
 * - Given a number, the result is an array with one element,
 *   containing that number
 * - Given a channel name (e g "FL") and a chmap,
 *   it will look this up in the chmap and return all matches
 * - Given a channel name and no chmap, the result is an array with one element,
     containing alsa standard channel map. Note that this might be a negative
     number in case of "UNKNOWN", "NA" or "MONO".
 * Return value is number of matches written.
 */
static int strtochannel(const char *id, snd_pcm_chmap_t *chmap,
			 long *channel, int channel_size)
{
	int ch;
	if (safe_strtol(id, channel) >= 0)
		return 1;

	ch = (int) snd_pcm_chmap_from_string(id);
	if (ch == -1)
		return -EINVAL;

	if (chmap) {
		int i, r = 0;
		/* Start with highest channel to simplify implementation of
		   determine ttable size */
		for (i = chmap->channels - 1; i >= 0; i--) {
			if ((int) chmap->pos[i] != ch)
				continue;
			if (r >= channel_size)
				continue;
			channel[r++] = i;
		}
		return r;
	}
	else {
		/* Assume ALSA standard channel mapping */
		*channel = ch - SND_CHMAP_FL;
		return 1;
	}
}

#define MAX_CHMAP_CHANNELS 256

static int determine_chmap(snd_config_t *tt, snd_pcm_chmap_t **tt_chmap)
{
	snd_config_iterator_t i, inext;
	snd_pcm_chmap_t *chmap;

	assert(tt && tt_chmap);
	chmap = malloc(sizeof(snd_pcm_chmap_t) +
		       MAX_CHMAP_CHANNELS * sizeof(unsigned int));

	chmap->channels = 0;
	snd_config_for_each(i, inext, tt) {
		const char *id;
		snd_config_iterator_t j, jnext;
		snd_config_t *in = snd_config_iterator_entry(i);

		if (snd_config_get_id(in, &id) < 0)
			continue;
		if (snd_config_get_type(in) != SND_CONFIG_TYPE_COMPOUND)
			goto err;
		snd_config_for_each(j, jnext, in) {
			int ch, k, found;
			long schannel;
			snd_config_t *jnode = snd_config_iterator_entry(j);
			if (snd_config_get_id(jnode, &id) < 0)
				continue;
			if (safe_strtol(id, &schannel) >= 0)
				continue;
			ch = (int) snd_pcm_chmap_from_string(id);
			if (ch == -1)
				goto err;

			found = 0;
			for (k = 0; k < (int) chmap->channels; k++)
				if (ch == (int) chmap->pos[k]) {
					found = 1;
					break;
				}
			if (found)
				continue;

			if (chmap->channels >= MAX_CHMAP_CHANNELS) {
				SNDERR("Too many channels in ttable chmap");
				goto err;
			}
			chmap->pos[chmap->channels++] = ch;
		}
	}

	if (chmap->channels == 0) {
		free(chmap);
		chmap = NULL;
	}
	*tt_chmap = chmap;
	return 0;

err:
	*tt_chmap = NULL;
	free(chmap);
	return -EINVAL;
}

static int find_matching_chmap(snd_pcm_t *spcm, snd_pcm_chmap_t *tt_chmap,
			       snd_pcm_chmap_t **found_chmap, int *schannels)
{
	snd_pcm_chmap_query_t** chmaps = snd_pcm_query_chmaps(spcm);
	int i;

	*found_chmap = NULL;

	if (chmaps == NULL)
		return 0; /* chmap API not supported for this slave */

	for (i = 0; chmaps[i]; i++) {
		unsigned int j, k;
		int match = 1;
		snd_pcm_chmap_t *c = &chmaps[i]->map;
		if (*schannels >= 0 && (int) c->channels != *schannels)
			continue;

		for (j = 0; j < tt_chmap->channels; j++) {
			int found = 0;
			unsigned int ch = tt_chmap->pos[j];
			for (k = 0; k < c->channels; k++)
				if (c->pos[k] == ch) {
					found = 1;
					break;
				}
			if (!found) {
				match = 0;
				break;
			}
		}

		if (match) {
			int size = sizeof(snd_pcm_chmap_t) + c->channels * sizeof(unsigned int);
			*found_chmap = malloc(size);
			if (!*found_chmap) {
				snd_pcm_free_chmaps(chmaps);
				return -ENOMEM;
			}
			memcpy(*found_chmap, c, size);
			*schannels = c->channels;
			break;
		}
	}

	snd_pcm_free_chmaps(chmaps);

	if (*found_chmap == NULL) {
		SNDERR("Found no matching channel map");
		return -EINVAL;
	}
	return 0;
}

static int route_chmap_init(snd_pcm_t *pcm)
{
	int set_map = 0;
	snd_pcm_chmap_t *current;
	snd_pcm_route_t *route = pcm->private_data;
	if (!route->chmap)
		return 0;
	if (__snd_pcm_state(pcm) != SND_PCM_STATE_PREPARED)
		return 0;

	/* Check if we really need to set the chmap or not.
	   This is important in case set_chmap is not implemented. */
	current = snd_pcm_get_chmap(route->plug.gen.slave);
	if (!current)
		return -ENOSYS;
	if (current->channels != route->chmap->channels)
		set_map = 1;
	else
		set_map = memcmp(current->pos, route->chmap->pos,
				 current->channels);
	free(current);
	if (!set_map)
		return 0;

	return snd_pcm_set_chmap(route->plug.gen.slave, route->chmap);
}


static const snd_pcm_ops_t snd_pcm_route_ops = {
	.close = snd_pcm_route_close,
	.info = snd_pcm_generic_info,
	.hw_refine = snd_pcm_route_hw_refine,
	.hw_params = snd_pcm_route_hw_params,
	.hw_free = snd_pcm_generic_hw_free,
	.sw_params = snd_pcm_generic_sw_params,
	.channel_info = snd_pcm_generic_channel_info,
	.dump = snd_pcm_route_dump,
	.nonblock = snd_pcm_generic_nonblock,
	.async = snd_pcm_generic_async,
	.mmap = snd_pcm_generic_mmap,
	.munmap = snd_pcm_generic_munmap,
	.query_chmaps = snd_pcm_route_query_chmaps,
	.get_chmap = snd_pcm_route_get_chmap,
	.set_chmap = NULL, /* NYI */
};

static int route_load_ttable(snd_pcm_route_params_t *params, snd_pcm_stream_t stream,
			     unsigned int tt_ssize,
			     snd_pcm_route_ttable_entry_t *ttable,
			     unsigned int tt_cused, unsigned int tt_sused)
{
	unsigned int src_channel, dst_channel;
	snd_pcm_route_ttable_dst_t *dptr;
	unsigned int sused, dused, smul, dmul;
	if (stream == SND_PCM_STREAM_PLAYBACK) {
		sused = tt_cused;
		dused = tt_sused;
		smul = tt_ssize;
		dmul = 1;
	} else {
		sused = tt_sused;
		dused = tt_cused;
		smul = 1;
		dmul = tt_ssize;
	}
	params->ndsts = dused;
	params->nsrcs = sused;
	dptr = calloc(dused, sizeof(*params->dsts));
	if (!dptr)
		return -ENOMEM;
	params->dsts = dptr;
	for (dst_channel = 0; dst_channel < dused; ++dst_channel) {
		snd_pcm_route_ttable_entry_t t = 0;
		int att = 0;
		int nsrcs = 0;
		snd_pcm_route_ttable_src_t srcs[sused];
		for (src_channel = 0; src_channel < sused; ++src_channel) {
			snd_pcm_route_ttable_entry_t v;
			v = ttable[src_channel * smul + dst_channel * dmul];
			if (v != 0) {
				srcs[nsrcs].channel = src_channel;
#if SND_PCM_PLUGIN_ROUTE_FLOAT
				/* Also in user space for non attenuated */
				srcs[nsrcs].as_int = (v == SND_PCM_PLUGIN_ROUTE_FULL ? SND_PCM_PLUGIN_ROUTE_RESOLUTION : 0);
				srcs[nsrcs].as_float = v;
#else
				assert(v >= 0 && v <= SND_PCM_PLUGIN_ROUTE_FULL);
				srcs[nsrcs].as_int = v;
#endif
				if (v != SND_PCM_PLUGIN_ROUTE_FULL)
					att = 1;
				t += v;
				nsrcs++;
			}
		}
#if 0
		assert(t <= SND_PCM_PLUGIN_ROUTE_FULL);
#endif
		dptr->att = att;
		dptr->nsrcs = nsrcs;
		if (nsrcs == 0)
			dptr->func = snd_pcm_route_convert1_zero;
		else
			dptr->func = snd_pcm_route_convert1_many;
		if (nsrcs > 0) {
			dptr->srcs = calloc((unsigned int) nsrcs, sizeof(*srcs));
			if (!dptr->srcs)
				return -ENOMEM;
			memcpy(dptr->srcs, srcs, sizeof(*srcs) * nsrcs);
		} else
			dptr->srcs = 0;
		dptr++;
	}
	return 0;
}

/**
 * \brief Creates a new Route & Volume PCM
 * \param pcmp Returns created PCM handle
 * \param name Name of PCM
 * \param sformat Slave format
 * \param schannels Slave channels
 * \param ttable Attenuation table
 * \param tt_ssize Attenuation table - slave size
 * \param tt_cused Attenuation table - client used count
 * \param tt_sused Attenuation table - slave used count
 * \param slave Slave PCM handle
 * \param close_slave When set, the slave PCM handle is closed with copy PCM
 * \retval zero on success otherwise a negative error code
 * \warning Using of this function might be dangerous in the sense
 *          of compatibility reasons. The prototype might be freely
 *          changed in future.
 */
int snd_pcm_route_open(snd_pcm_t **pcmp, const char *name,
		       snd_pcm_format_t sformat, int schannels,
		       snd_pcm_route_ttable_entry_t *ttable,
		       unsigned int tt_ssize,
		       unsigned int tt_cused, unsigned int tt_sused,
		       snd_pcm_t *slave, int close_slave)
{
	snd_pcm_t *pcm;
	snd_pcm_route_t *route;
	int err;
	assert(pcmp && slave && ttable);
	if (sformat != SND_PCM_FORMAT_UNKNOWN && 
	    snd_pcm_format_linear(sformat) != 1)
		return -EINVAL;
	route = calloc(1, sizeof(snd_pcm_route_t));
	if (!route) {
		return -ENOMEM;
	}
	snd_pcm_plugin_init(&route->plug);
	route->sformat = sformat;
	route->schannels = schannels;
	route->plug.read = snd_pcm_route_read_areas;
	route->plug.write = snd_pcm_route_write_areas;
	route->plug.undo_read = snd_pcm_plugin_undo_read_generic;
	route->plug.undo_write = snd_pcm_plugin_undo_write_generic;
	route->plug.gen.slave = slave;
	route->plug.gen.close_slave = close_slave;
	route->plug.init = route_chmap_init;

	err = snd_pcm_new(&pcm, SND_PCM_TYPE_ROUTE, name, slave->stream, slave->mode);
	if (err < 0) {
		free(route);
		return err;
	}
	pcm->ops = &snd_pcm_route_ops;
	pcm->fast_ops = &snd_pcm_plugin_fast_ops;
	pcm->private_data = route;
	pcm->poll_fd = slave->poll_fd;
	pcm->poll_events = slave->poll_events;
	pcm->tstamp_type = slave->tstamp_type;
	snd_pcm_set_hw_ptr(pcm, &route->plug.hw_ptr, -1, 0);
	snd_pcm_set_appl_ptr(pcm, &route->plug.appl_ptr, -1, 0);
	err = route_load_ttable(&route->params, pcm->stream, tt_ssize, ttable, tt_cused, tt_sused);
	if (err < 0) {
		snd_pcm_close(pcm);
		return err;
	}
	*pcmp = pcm;

	return 0;
}

static int _snd_pcm_route_determine_ttable(snd_config_t *tt,
					   unsigned int *tt_csize,
					   unsigned int *tt_ssize,
					   snd_pcm_chmap_t *chmap)
{
	snd_config_iterator_t i, inext;
	long csize = 0, ssize = 0;
	int err;

	assert(tt && tt_csize && tt_ssize);
	snd_config_for_each(i, inext, tt) {
		snd_config_t *in = snd_config_iterator_entry(i);
		snd_config_iterator_t j, jnext;
		long cchannel;
		const char *id;
		if (snd_config_get_id(in, &id) < 0)
			continue;
		err = safe_strtol(id, &cchannel);
		if (err < 0) {
			SNDERR("Invalid client channel: %s", id);
			return -EINVAL;
		}
		if (cchannel + 1 > csize)
			csize = cchannel + 1;
		if (snd_config_get_type(in) != SND_CONFIG_TYPE_COMPOUND)
			return -EINVAL;
		snd_config_for_each(j, jnext, in) {
			snd_config_t *jnode = snd_config_iterator_entry(j);
			long schannel;
			const char *id;
			if (snd_config_get_id(jnode, &id) < 0)
				continue;
			err = strtochannel(id, chmap, &schannel, 1);
			if (err < 0) {
				SNDERR("Invalid slave channel: %s", id);
				return -EINVAL;
			}
			if (schannel + 1 > ssize)
				ssize = schannel + 1;
		}
	}
	if (csize == 0 || ssize == 0) {
		SNDERR("Invalid null ttable configuration");
		return -EINVAL;
	}
	*tt_csize = csize;
	*tt_ssize = ssize;
	return 0;
}

/**
 * \brief Determine route matrix sizes
 * \param tt Configuration root describing route matrix
 * \param tt_csize Returned client size in elements
 * \param tt_ssize Returned slave size in elements
 * \retval zero on success otherwise a negative error code
 */
int snd_pcm_route_determine_ttable(snd_config_t *tt,
				   unsigned int *tt_csize,
				   unsigned int *tt_ssize)
{
	return _snd_pcm_route_determine_ttable(tt, tt_csize, tt_ssize, NULL);
}

/**
 * \brief Load route matrix
 * \param tt Configuration root describing route matrix
 * \param ttable Returned route matrix
 * \param tt_csize Client size in elements
 * \param tt_ssize Slave size in elements
 * \param tt_cused Used client elements
 * \param tt_sused Used slave elements
 * \param schannels Slave channels
 * \retval zero on success otherwise a negative error code
 */
static int _snd_pcm_route_load_ttable(snd_config_t *tt, snd_pcm_route_ttable_entry_t *ttable,
				      unsigned int tt_csize, unsigned int tt_ssize,
				      unsigned int *tt_cused, unsigned int *tt_sused,
				      int schannels, snd_pcm_chmap_t *chmap)
{
	int cused = -1;
	int sused = -1;
	snd_config_iterator_t i, inext;
	unsigned int k;
	int err;
	for (k = 0; k < tt_csize * tt_ssize; ++k)
		ttable[k] = 0.0;
	snd_config_for_each(i, inext, tt) {
		snd_config_t *in = snd_config_iterator_entry(i);
		snd_config_iterator_t j, jnext;
		long cchannel;
		const char *id;
		if (snd_config_get_id(in, &id) < 0)
			continue;
		err = safe_strtol(id, &cchannel);
		if (err < 0 || 
		    cchannel < 0 || (unsigned int) cchannel > tt_csize) {
			SNDERR("Invalid client channel: %s", id);
			return -EINVAL;
		}
		if (snd_config_get_type(in) != SND_CONFIG_TYPE_COMPOUND)
			return -EINVAL;
		snd_config_for_each(j, jnext, in) {
			snd_config_t *jnode = snd_config_iterator_entry(j);
			double value;
			int ss;
			long *scha = alloca(tt_ssize * sizeof(long));
			const char *id;
			if (snd_config_get_id(jnode, &id) < 0)
				continue;

			ss = strtochannel(id, chmap, scha, tt_ssize);
			if (ss < 0) {
				SNDERR("Invalid slave channel: %s", id);
				return -EINVAL;
			}

			err = snd_config_get_real(jnode, &value);
			if (err < 0) {
				long v;
				err = snd_config_get_integer(jnode, &v);
				if (err < 0) {
					SNDERR("Invalid type for %s", id);
					return -EINVAL;
				}
				value = v;
			}

			for (k = 0; (int) k < ss; k++) {
				long schannel = scha[k];
				if (schannel < 0 || (unsigned int) schannel > tt_ssize ||
				    (schannels > 0 && schannel >= schannels)) {
					SNDERR("Invalid slave channel: %s", id);
					return -EINVAL;
				}
				ttable[cchannel * tt_ssize + schannel] = value;
				if (schannel > sused)
					sused = schannel;
			}
		}
		if (cchannel > cused)
			cused = cchannel;
	}
	*tt_sused = sused + 1;
	*tt_cused = cused + 1;
	return 0;
}

/**
 * \brief Load route matrix
 * \param tt Configuration root describing route matrix
 * \param ttable Returned route matrix
 * \param tt_csize Client size in elements
 * \param tt_ssize Slave size in elements
 * \param tt_cused Used client elements
 * \param tt_sused Used slave elements
 * \param schannels Slave channels
 * \retval zero on success otherwise a negative error code
 */
int snd_pcm_route_load_ttable(snd_config_t *tt, snd_pcm_route_ttable_entry_t *ttable,
			      unsigned int tt_csize, unsigned int tt_ssize,
			      unsigned int *tt_cused, unsigned int *tt_sused,
			      int schannels)
{
	return _snd_pcm_route_load_ttable(tt, ttable, tt_csize, tt_ssize,
					  tt_cused, tt_sused, schannels, NULL);
}

/*! \page pcm_plugins

\section pcm_plugins_route Plugin: Route & Volume

This plugin converts channels and applies volume during the conversion.
The format and rate must match for both of them.

SCHANNEL can be a channel name instead of a number (e g FL, LFE).
If so, a matching channel map will be selected for the slave.

\code
pcm.name {
        type route              # Route & Volume conversion PCM
        slave STR               # Slave name
        # or
        slave {                 # Slave definition
                pcm STR         # Slave PCM name
                # or
                pcm { }         # Slave PCM definition
                [format STR]    # Slave format
                [channels INT]  # Slave channels
        }
        ttable {                # Transfer table (bi-dimensional compound of cchannels * schannels numbers)
                CCHANNEL {
                        SCHANNEL REAL   # route value (0.0 - 1.0)
                }
        }
}
\endcode

\subsection pcm_plugins_route_funcref Function reference

<UL>
  <LI>snd_pcm_route_open()
  <LI>_snd_pcm_route_open()
</UL>

*/

/**
 * \brief Creates a new Route & Volume PCM
 * \param pcmp Returns created PCM handle
 * \param name Name of PCM
 * \param root Root configuration node
 * \param conf Configuration node with Route & Volume PCM description
 * \param stream Stream type
 * \param mode Stream mode
 * \retval zero on success otherwise a negative error code
 * \warning Using of this function might be dangerous in the sense
 *          of compatibility reasons. The prototype might be freely
 *          changed in future.
 */
int _snd_pcm_route_open(snd_pcm_t **pcmp, const char *name,
			snd_config_t *root, snd_config_t *conf, 
			snd_pcm_stream_t stream, int mode)
{
	snd_config_iterator_t i, next;
	int err;
	snd_pcm_t *spcm;
	snd_config_t *slave = NULL, *sconf;
	snd_pcm_chmap_t *tt_chmap = NULL, *chmap = NULL;
	snd_pcm_format_t sformat = SND_PCM_FORMAT_UNKNOWN;
	int schannels = -1;
	snd_config_t *tt = NULL;
	snd_pcm_route_ttable_entry_t *ttable = NULL;
	unsigned int csize, ssize;
	unsigned int cused, sused;
	snd_config_for_each(i, next, conf) {
		snd_config_t *n = snd_config_iterator_entry(i);
		const char *id;
		if (snd_config_get_id(n, &id) < 0)
			continue;
		if (snd_pcm_conf_generic_id(id))
			continue;
		if (strcmp(id, "slave") == 0) {
			slave = n;
			continue;
		}
		if (strcmp(id, "ttable") == 0) {
			if (snd_config_get_type(n) != SND_CONFIG_TYPE_COMPOUND) {
				SNDERR("Invalid type for %s", id);
				return -EINVAL;
			}
			tt = n;
			continue;
		}
		SNDERR("Unknown field %s", id);
		return -EINVAL;
	}
	if (!slave) {
		SNDERR("slave is not defined");
		return -EINVAL;
	}
	if (!tt) {
		SNDERR("ttable is not defined");
		return -EINVAL;
	}
	err = snd_pcm_slave_conf(root, slave, &sconf, 2,
				 SND_PCM_HW_PARAM_FORMAT, 0, &sformat,
				 SND_PCM_HW_PARAM_CHANNELS, 0, &schannels);
	if (err < 0)
		return err;
	if (sformat != SND_PCM_FORMAT_UNKNOWN &&
	    snd_pcm_format_linear(sformat) != 1) {
	    	snd_config_delete(sconf);
		SNDERR("slave format is not linear");
		return -EINVAL;
	}

	err = determine_chmap(tt, &tt_chmap);
	if (err < 0) {
		free(ttable);
		return err;
	}

	err = snd_pcm_open_slave(&spcm, root, sconf, stream, mode, conf);
	snd_config_delete(sconf);
	if (err < 0) {
		free(tt_chmap);
		free(ttable);
		return err;
	}

	if (tt_chmap) {
		err = find_matching_chmap(spcm, tt_chmap, &chmap, &schannels);
		free(tt_chmap);
		if (err < 0) {
			snd_pcm_close(spcm);
			return err;
		}
	}

	err = _snd_pcm_route_determine_ttable(tt, &csize, &ssize, chmap);
	if (err < 0) {
		free(chmap);
		snd_pcm_close(spcm);
		return err;
	}
	ttable = malloc(csize * ssize * sizeof(snd_pcm_route_ttable_entry_t));
	if (ttable == NULL) {
		free(chmap);
		snd_pcm_close(spcm);
		return -ENOMEM;
	}
	err = _snd_pcm_route_load_ttable(tt, ttable, csize, ssize,
					&cused, &sused, schannels, chmap);
	if (err < 0) {
		free(chmap);
		free(ttable);
		snd_pcm_close(spcm);
		return err;
	}

	err = snd_pcm_route_open(pcmp, name, sformat, schannels,
				 ttable, ssize,
				 cused, sused,
				 spcm, 1);
	free(ttable);
	if (err < 0) {
		free(chmap);
		snd_pcm_close(spcm);
	} else {
		((snd_pcm_route_t*) (*pcmp)->private_data)->chmap = chmap;
	}

	return err;
}
#ifndef DOC_HIDDEN
SND_DLSYM_BUILD_VERSION(_snd_pcm_route_open, SND_PCM_DLSYM_VERSION);
#endif
