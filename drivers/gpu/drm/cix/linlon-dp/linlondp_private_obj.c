// SPDX-License-Identifier: GPL-2.0
/*
 * (C) COPYRIGHT 2022-2023 Arm Technology (China) Co., Ltd.
 * ALL RIGHTS RESERVED
 *
 */
#include "linlondp_dev.h"
#include "linlondp_kms.h"

#if KERNEL_VERSION(7, 1, 0) <= LINUX_VERSION_CODE
/* drm_atomic_private_obj_init() became 3-arg in v7.1 and now allocates the
 * initial private-object state via funcs->atomic_create_state(). Older
 * kernels take a driver-allocated state as the 3rd argument instead, so the
 * create_state helpers and to_component() below are only built on v7.1+.
 */
#define to_component(o) container_of((o), struct linlondp_component, obj)
#endif

static void linlondp_component_state_reset(struct linlondp_component_state *st)
{
	st->binding_user = NULL;
	st->affected_inputs = st->active_inputs;
	st->active_inputs = 0;
	st->changed_active_inputs = 0;
}

static struct drm_private_state *
linlondp_layer_atomic_duplicate_state(struct drm_private_obj *obj)
{
	struct linlondp_layer_state *old =
		to_layer_st(priv_to_comp_st(obj->state));
	struct linlondp_layer_state *st;

	st = kmemdup(obj->state, sizeof(*st), GFP_KERNEL);
	if (!st)
		return NULL;

	linlondp_color_duplicate_state(&st->color_st, &old->color_st);

	linlondp_component_state_reset(&st->base);
	__drm_atomic_helper_private_obj_duplicate_state(obj, &st->base.obj);

	return &st->base.obj;
}

static void linlondp_layer_atomic_destroy_state(struct drm_private_obj *obj,
						struct drm_private_state *state)
{
	struct linlondp_layer_state *st = to_layer_st(priv_to_comp_st(state));

	kfree(st);
}

#if KERNEL_VERSION(7, 1, 0) <= LINUX_VERSION_CODE
static struct drm_private_state *
linlondp_layer_atomic_create_state(struct drm_private_obj *obj)
{
	struct linlondp_layer_state *st;

	st = kzalloc_obj(*st, GFP_KERNEL);
	if (!st)
		return ERR_PTR(-ENOMEM);

	__drm_atomic_helper_private_obj_create_state(obj, &st->base.obj);
	linlondp_component_state_reset(&st->base);
	st->base.component = to_component(obj);

	return &st->base.obj;
}
#endif

static const struct drm_private_state_funcs linlondp_layer_obj_funcs = {
#if KERNEL_VERSION(7, 1, 0) <= LINUX_VERSION_CODE
	.atomic_create_state = linlondp_layer_atomic_create_state,
#endif
	.atomic_duplicate_state = linlondp_layer_atomic_duplicate_state,
	.atomic_destroy_state = linlondp_layer_atomic_destroy_state,
};

static int linlondp_layer_obj_add(struct linlondp_kms_dev *kms,
				  struct linlondp_layer *layer)
{
#if KERNEL_VERSION(7, 1, 0) > LINUX_VERSION_CODE
	struct linlondp_layer_state *st;

	st = kzalloc(sizeof(*st), GFP_KERNEL);
	if (!st)
		return -ENOMEM;

	st->base.component = &layer->base;
	drm_atomic_private_obj_init(&kms->base, &layer->base.obj, &st->base.obj,
				    &linlondp_layer_obj_funcs);
#else
	drm_atomic_private_obj_init(&kms->base, &layer->base.obj,
				    &linlondp_layer_obj_funcs);
#endif
	return 0;
}

static struct drm_private_state *
linlondp_scaler_atomic_duplicate_state(struct drm_private_obj *obj)
{
	struct linlondp_scaler_state *st;

	st = kmemdup(obj->state, sizeof(*st), GFP_KERNEL);
	if (!st)
		return NULL;

	linlondp_component_state_reset(&st->base);
	__drm_atomic_helper_private_obj_duplicate_state(obj, &st->base.obj);

	return &st->base.obj;
}

static void
linlondp_scaler_atomic_destroy_state(struct drm_private_obj *obj,
				     struct drm_private_state *state)
{
	kfree(to_scaler_st(priv_to_comp_st(state)));
}

#if KERNEL_VERSION(7, 1, 0) <= LINUX_VERSION_CODE
static struct drm_private_state *
linlondp_scaler_atomic_create_state(struct drm_private_obj *obj)
{
	struct linlondp_scaler_state *st;

	st = kzalloc_obj(*st, GFP_KERNEL);
	if (!st)
		return ERR_PTR(-ENOMEM);

	__drm_atomic_helper_private_obj_create_state(obj, &st->base.obj);
	linlondp_component_state_reset(&st->base);
	st->base.component = to_component(obj);

	return &st->base.obj;
}
#endif

static const struct drm_private_state_funcs linlondp_scaler_obj_funcs = {
#if KERNEL_VERSION(7, 1, 0) <= LINUX_VERSION_CODE
	.atomic_create_state = linlondp_scaler_atomic_create_state,
#endif
	.atomic_duplicate_state = linlondp_scaler_atomic_duplicate_state,
	.atomic_destroy_state = linlondp_scaler_atomic_destroy_state,
};

static int linlondp_scaler_obj_add(struct linlondp_kms_dev *kms,
				   struct linlondp_scaler *scaler)
{
#if KERNEL_VERSION(7, 1, 0) > LINUX_VERSION_CODE
	struct linlondp_scaler_state *st;

	st = kzalloc(sizeof(*st), GFP_KERNEL);
	if (!st)
		return -ENOMEM;

	st->base.component = &scaler->base;
	drm_atomic_private_obj_init(&kms->base, &scaler->base.obj, &st->base.obj,
				    &linlondp_scaler_obj_funcs);
#else
	drm_atomic_private_obj_init(&kms->base, &scaler->base.obj,
				    &linlondp_scaler_obj_funcs);
#endif
	return 0;
}

static struct drm_private_state *
linlondp_compiz_atomic_duplicate_state(struct drm_private_obj *obj)
{
	struct linlondp_compiz_state *st;

	st = kmemdup(obj->state, sizeof(*st), GFP_KERNEL);
	if (!st)
		return NULL;

	linlondp_component_state_reset(&st->base);
	__drm_atomic_helper_private_obj_duplicate_state(obj, &st->base.obj);

	return &st->base.obj;
}

static void
linlondp_compiz_atomic_destroy_state(struct drm_private_obj *obj,
				     struct drm_private_state *state)
{
	kfree(to_compiz_st(priv_to_comp_st(state)));
}

#if KERNEL_VERSION(7, 1, 0) <= LINUX_VERSION_CODE
static struct drm_private_state *
linlondp_compiz_atomic_create_state(struct drm_private_obj *obj)
{
	struct linlondp_compiz_state *st;

	st = kzalloc_obj(*st, GFP_KERNEL);
	if (!st)
		return ERR_PTR(-ENOMEM);

	__drm_atomic_helper_private_obj_create_state(obj, &st->base.obj);
	linlondp_component_state_reset(&st->base);
	st->base.component = to_component(obj);

	return &st->base.obj;
}
#endif

static const struct drm_private_state_funcs linlondp_compiz_obj_funcs = {
#if KERNEL_VERSION(7, 1, 0) <= LINUX_VERSION_CODE
	.atomic_create_state = linlondp_compiz_atomic_create_state,
#endif
	.atomic_duplicate_state = linlondp_compiz_atomic_duplicate_state,
	.atomic_destroy_state = linlondp_compiz_atomic_destroy_state,
};

static int linlondp_compiz_obj_add(struct linlondp_kms_dev *kms,
				   struct linlondp_compiz *compiz)
{
#if KERNEL_VERSION(7, 1, 0) > LINUX_VERSION_CODE
	struct linlondp_compiz_state *st;

	st = kzalloc(sizeof(*st), GFP_KERNEL);
	if (!st)
		return -ENOMEM;

	st->base.component = &compiz->base;
	drm_atomic_private_obj_init(&kms->base, &compiz->base.obj, &st->base.obj,
				    &linlondp_compiz_obj_funcs);
#else
	drm_atomic_private_obj_init(&kms->base, &compiz->base.obj,
				    &linlondp_compiz_obj_funcs);
#endif
	return 0;
}

static struct drm_private_state *
linlondp_splitter_atomic_duplicate_state(struct drm_private_obj *obj)
{
	struct linlondp_splitter_state *st;

	st = kmemdup(obj->state, sizeof(*st), GFP_KERNEL);
	if (!st)
		return NULL;

	linlondp_component_state_reset(&st->base);
	__drm_atomic_helper_private_obj_duplicate_state(obj, &st->base.obj);

	return &st->base.obj;
}

static void
linlondp_splitter_atomic_destroy_state(struct drm_private_obj *obj,
				       struct drm_private_state *state)
{
	kfree(to_splitter_st(priv_to_comp_st(state)));
}

#if KERNEL_VERSION(7, 1, 0) <= LINUX_VERSION_CODE
static struct drm_private_state *
linlondp_splitter_atomic_create_state(struct drm_private_obj *obj)
{
	struct linlondp_splitter_state *st;

	st = kzalloc_obj(*st, GFP_KERNEL);
	if (!st)
		return ERR_PTR(-ENOMEM);

	__drm_atomic_helper_private_obj_create_state(obj, &st->base.obj);
	linlondp_component_state_reset(&st->base);
	st->base.component = to_component(obj);

	return &st->base.obj;
}
#endif

static const struct drm_private_state_funcs linlondp_splitter_obj_funcs = {
#if KERNEL_VERSION(7, 1, 0) <= LINUX_VERSION_CODE
	.atomic_create_state = linlondp_splitter_atomic_create_state,
#endif
	.atomic_duplicate_state = linlondp_splitter_atomic_duplicate_state,
	.atomic_destroy_state = linlondp_splitter_atomic_destroy_state,
};

static int linlondp_splitter_obj_add(struct linlondp_kms_dev *kms,
				     struct linlondp_splitter *splitter)
{
#if KERNEL_VERSION(7, 1, 0) > LINUX_VERSION_CODE
	struct linlondp_splitter_state *st;

	st = kzalloc(sizeof(*st), GFP_KERNEL);
	if (!st)
		return -ENOMEM;

	st->base.component = &splitter->base;
	drm_atomic_private_obj_init(&kms->base, &splitter->base.obj, &st->base.obj,
				    &linlondp_splitter_obj_funcs);
#else
	drm_atomic_private_obj_init(&kms->base, &splitter->base.obj,
				    &linlondp_splitter_obj_funcs);
#endif
	return 0;
}

static struct drm_private_state *
linlondp_merger_atomic_duplicate_state(struct drm_private_obj *obj)
{
	struct linlondp_merger_state *st;

	st = kmemdup(obj->state, sizeof(*st), GFP_KERNEL);
	if (!st)
		return NULL;

	linlondp_component_state_reset(&st->base);
	__drm_atomic_helper_private_obj_duplicate_state(obj, &st->base.obj);

	return &st->base.obj;
}

static void
linlondp_merger_atomic_destroy_state(struct drm_private_obj *obj,
				     struct drm_private_state *state)
{
	kfree(to_merger_st(priv_to_comp_st(state)));
}

#if KERNEL_VERSION(7, 1, 0) <= LINUX_VERSION_CODE
static struct drm_private_state *
linlondp_merger_atomic_create_state(struct drm_private_obj *obj)
{
	struct linlondp_merger_state *st;

	st = kzalloc_obj(*st, GFP_KERNEL);
	if (!st)
		return ERR_PTR(-ENOMEM);

	__drm_atomic_helper_private_obj_create_state(obj, &st->base.obj);
	linlondp_component_state_reset(&st->base);
	st->base.component = to_component(obj);

	return &st->base.obj;
}
#endif

static const struct drm_private_state_funcs linlondp_merger_obj_funcs = {
#if KERNEL_VERSION(7, 1, 0) <= LINUX_VERSION_CODE
	.atomic_create_state = linlondp_merger_atomic_create_state,
#endif
	.atomic_duplicate_state = linlondp_merger_atomic_duplicate_state,
	.atomic_destroy_state = linlondp_merger_atomic_destroy_state,
};

static int linlondp_merger_obj_add(struct linlondp_kms_dev *kms,
				   struct linlondp_merger *merger)
{
#if KERNEL_VERSION(7, 1, 0) > LINUX_VERSION_CODE
	struct linlondp_merger_state *st;

	st = kzalloc(sizeof(*st), GFP_KERNEL);
	if (!st)
		return -ENOMEM;

	st->base.component = &merger->base;
	drm_atomic_private_obj_init(&kms->base, &merger->base.obj, &st->base.obj,
				    &linlondp_merger_obj_funcs);
#else
	drm_atomic_private_obj_init(&kms->base, &merger->base.obj,
				    &linlondp_merger_obj_funcs);
#endif
	return 0;
}

static struct drm_private_state *
linlondp_improc_atomic_duplicate_state(struct drm_private_obj *obj)
{
	struct linlondp_improc_state *st;

	st = kmemdup(obj->state, sizeof(*st), GFP_KERNEL);
	if (!st)
		return NULL;

	linlondp_component_state_reset(&st->base);
	__drm_atomic_helper_private_obj_duplicate_state(obj, &st->base.obj);

	return &st->base.obj;
}

static void
linlondp_improc_atomic_destroy_state(struct drm_private_obj *obj,
				     struct drm_private_state *state)
{
	kfree(to_improc_st(priv_to_comp_st(state)));
}

#if KERNEL_VERSION(7, 1, 0) <= LINUX_VERSION_CODE
static struct drm_private_state *
linlondp_improc_atomic_create_state(struct drm_private_obj *obj)
{
	struct linlondp_improc_state *st;

	st = kzalloc_obj(*st, GFP_KERNEL);
	if (!st)
		return ERR_PTR(-ENOMEM);

	__drm_atomic_helper_private_obj_create_state(obj, &st->base.obj);
	linlondp_component_state_reset(&st->base);
	st->base.component = to_component(obj);

	return &st->base.obj;
}
#endif

static const struct drm_private_state_funcs linlondp_improc_obj_funcs = {
#if KERNEL_VERSION(7, 1, 0) <= LINUX_VERSION_CODE
	.atomic_create_state = linlondp_improc_atomic_create_state,
#endif
	.atomic_duplicate_state = linlondp_improc_atomic_duplicate_state,
	.atomic_destroy_state = linlondp_improc_atomic_destroy_state,
};

static int linlondp_improc_obj_add(struct linlondp_kms_dev *kms,
				   struct linlondp_improc *improc)
{
#if KERNEL_VERSION(7, 1, 0) > LINUX_VERSION_CODE
	struct linlondp_improc_state *st;

	st = kzalloc(sizeof(*st), GFP_KERNEL);
	if (!st)
		return -ENOMEM;

	st->base.component = &improc->base;
	drm_atomic_private_obj_init(&kms->base, &improc->base.obj, &st->base.obj,
				    &linlondp_improc_obj_funcs);
#else
	drm_atomic_private_obj_init(&kms->base, &improc->base.obj,
				    &linlondp_improc_obj_funcs);
#endif
	return 0;
}

static struct drm_private_state *
linlondp_timing_ctrlr_atomic_duplicate_state(struct drm_private_obj *obj)
{
	struct linlondp_timing_ctrlr_state *st;

	st = kmemdup(obj->state, sizeof(*st), GFP_KERNEL);
	if (!st)
		return NULL;

	linlondp_component_state_reset(&st->base);
	__drm_atomic_helper_private_obj_duplicate_state(obj, &st->base.obj);

	return &st->base.obj;
}

static void
linlondp_timing_ctrlr_atomic_destroy_state(struct drm_private_obj *obj,
					   struct drm_private_state *state)
{
	kfree(to_ctrlr_st(priv_to_comp_st(state)));
}

#if KERNEL_VERSION(7, 1, 0) <= LINUX_VERSION_CODE
static struct drm_private_state *
linlondp_timing_ctrlr_atomic_create_state(struct drm_private_obj *obj)
{
	struct linlondp_timing_ctrlr_state *st;

	st = kzalloc_obj(*st, GFP_KERNEL);
	if (!st)
		return ERR_PTR(-ENOMEM);

	__drm_atomic_helper_private_obj_create_state(obj, &st->base.obj);
	linlondp_component_state_reset(&st->base);
	st->base.component = to_component(obj);

	return &st->base.obj;
}
#endif

static const struct drm_private_state_funcs linlondp_timing_ctrlr_obj_funcs = {
#if KERNEL_VERSION(7, 1, 0) <= LINUX_VERSION_CODE
	.atomic_create_state = linlondp_timing_ctrlr_atomic_create_state,
#endif
	.atomic_duplicate_state = linlondp_timing_ctrlr_atomic_duplicate_state,
	.atomic_destroy_state = linlondp_timing_ctrlr_atomic_destroy_state,
};

static int linlondp_timing_ctrlr_obj_add(struct linlondp_kms_dev *kms,
					 struct linlondp_timing_ctrlr *ctrlr)
{
#if KERNEL_VERSION(7, 1, 0) > LINUX_VERSION_CODE
	struct linlondp_timing_ctrlr_state *st;

	st = kzalloc(sizeof(*st), GFP_KERNEL);
	if (!st)
		return -ENOMEM;

	st->base.component = &ctrlr->base;
	drm_atomic_private_obj_init(&kms->base, &ctrlr->base.obj, &st->base.obj,
				    &linlondp_timing_ctrlr_obj_funcs);
#else
	drm_atomic_private_obj_init(&kms->base, &ctrlr->base.obj,
				    &linlondp_timing_ctrlr_obj_funcs);
#endif
	return 0;
}

static struct drm_private_state *
linlondp_pipeline_atomic_duplicate_state(struct drm_private_obj *obj)
{
	struct linlondp_pipeline_state *st;

	st = kmemdup(obj->state, sizeof(*st), GFP_KERNEL);
	if (!st)
		return NULL;

	st->active_comps = 0;

	__drm_atomic_helper_private_obj_duplicate_state(obj, &st->obj);

	return &st->obj;
}

static void
linlondp_pipeline_atomic_destroy_state(struct drm_private_obj *obj,
				       struct drm_private_state *state)
{
	kfree(priv_to_pipe_st(state));
}

#if KERNEL_VERSION(7, 1, 0) <= LINUX_VERSION_CODE
static struct drm_private_state *
linlondp_pipeline_atomic_create_state(struct drm_private_obj *obj)
{
	struct linlondp_pipeline_state *st;

	st = kzalloc_obj(*st, GFP_KERNEL);
	if (!st)
		return ERR_PTR(-ENOMEM);

	__drm_atomic_helper_private_obj_create_state(obj, &st->obj);
	st->active_comps = 0;
	st->pipe = container_of(obj, struct linlondp_pipeline, obj);

	return &st->obj;
}
#endif

static const struct drm_private_state_funcs linlondp_pipeline_obj_funcs = {
#if KERNEL_VERSION(7, 1, 0) <= LINUX_VERSION_CODE
	.atomic_create_state = linlondp_pipeline_atomic_create_state,
#endif
	.atomic_duplicate_state = linlondp_pipeline_atomic_duplicate_state,
	.atomic_destroy_state = linlondp_pipeline_atomic_destroy_state,
};

static int linlondp_pipeline_obj_add(struct linlondp_kms_dev *kms,
				     struct linlondp_pipeline *pipe)
{
#if KERNEL_VERSION(7, 1, 0) > LINUX_VERSION_CODE
	struct linlondp_pipeline_state *st;

	st = kzalloc(sizeof(*st), GFP_KERNEL);
	if (!st)
		return -ENOMEM;

	st->pipe = pipe;
	drm_atomic_private_obj_init(&kms->base, &pipe->obj, &st->obj,
				    &linlondp_pipeline_obj_funcs);
#else
	drm_atomic_private_obj_init(&kms->base, &pipe->obj,
				    &linlondp_pipeline_obj_funcs);
#endif
	return 0;
}

static int linlondp_kms_add_private_objs_one(struct linlondp_kms_dev *kms,
					     struct linlondp_dev *mdev)
{
	struct linlondp_pipeline *pipe;
	int i, j, err;

	for (i = 0; i < mdev->n_pipelines; i++) {
		pipe = mdev->pipelines[i];

		err = linlondp_pipeline_obj_add(kms, pipe);
		if (err)
			return err;

		for (j = 0; j < pipe->n_layers; j++) {
			err = linlondp_layer_obj_add(kms, pipe->layers[j]);
			if (err)
				return err;
		}

		if (pipe->wb_layer) {
			err = linlondp_layer_obj_add(kms, pipe->wb_layer);
			if (err)
				return err;
		}

		for (j = 0; j < pipe->n_scalers; j++) {
			err = linlondp_scaler_obj_add(kms, pipe->scalers[j]);
			if (err)
				return err;
		}

		err = linlondp_compiz_obj_add(kms, pipe->compiz);
		if (err)
			return err;

		if (pipe->splitter) {
			err = linlondp_splitter_obj_add(kms, pipe->splitter);
			if (err)
				return err;
		}

		if (pipe->merger) {
			err = linlondp_merger_obj_add(kms, pipe->merger);
			if (err)
				return err;
		}

		err = linlondp_improc_obj_add(kms, pipe->improc);
		if (err)
			return err;

		err = linlondp_timing_ctrlr_obj_add(kms, pipe->ctrlr);
		if (err)
			return err;
	}

	return 0;
}

int linlondp_kms_add_private_objs(struct linlondp_kms_dev *kms,
				  struct linlondp_dev *mdev)
{
	return linlondp_kms_add_private_objs_multi(kms, &mdev, 1);
}

int linlondp_kms_add_private_objs_multi(struct linlondp_kms_dev *kms,
					struct linlondp_dev **mdevs,
					unsigned int n_mdevs)
{
	unsigned int di;
	int err;

	for (di = 0; di < n_mdevs; di++) {
		err = linlondp_kms_add_private_objs_one(kms, mdevs[di]);
		if (err)
			return err;
	}

	return 0;
}

void linlondp_kms_cleanup_private_objs(struct linlondp_kms_dev *kms)
{
	struct drm_mode_config *config = &kms->base.mode_config;
	struct drm_private_obj *obj, *next;

	list_for_each_entry_safe(obj, next, &config->privobj_list, head)
		drm_atomic_private_obj_fini(obj);
}
