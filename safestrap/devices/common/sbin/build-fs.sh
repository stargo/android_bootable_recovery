#!/sbin/bbx sh
# By: Hashcode
PATH=/sbin:/system/xbin:/system/bin

# system/userdata/cache
IMAGE_NAME=`echo '${1}' | tr '[a-z]' '[A-Z]'`
LOOP_DEV=${2}
ROMSLOT_NAME=${3}
BBX=/sbin/bbx
SS_CONFIG=/ss.config
DISABLE_JOURNAL=


. /sbin/ss_function.sh
readConfig

eval CURRENT_BLOCK=\$BLOCK_${IMAGE_NAME}

$BBX rm $BLOCK_DIR/$CURRENT_BLOCK
$BBX ln -s $BLOCK_DIR/loop$LOOP_DEV $BLOCK_DIR/$CURRENT_BLOCK

if [ "$SS_USE_DATAMEDIA" = "1" ]; then
	DISABLE_JOURNAL="-O ^has_journal"
fi

if [ "$USERDATA_FSTYPE" = "f2fs" ] && [ "$LOOP_DEV" = "-userdata" ]; then
	mkfs.f2fs $BLOCK_DIR/loop$LOOP_DEV
else
	if [ "$LOOP_DEV" = "-userdata" ]; then
		mke2fs $DISABLE_JOURNAL -T $USERDATA_FSTYPE $BLOCK_DIR/loop$LOOP_DEV
	else
		mke2fs $DISABLE_JOURNAL -T $SYSTEM_FSTYPE $BLOCK_DIR/loop$LOOP_DEV
	fi
fi
# Ensure the system is the correct fs type after rom-slot creation
if [ "$SYATEM_FSTYPE" = "ext3" ] && [ "$LOOP_DEV" = "-system" ]; then
	mkfs.ext3 $BLOCK_DIR/loop$LOOP_DEV
else
	if [ "$LOOP_DEV" = "-system" ]; then
		mke2fs $DISABLE_JOURNAL -T $SYSTEM_FSTYPE $BLOCK_DIR/loop$LOOP_DEV
	fi
fi
#Same for userdata
if [ "$USERDATA_FSTYPE" = "ext3" ] && [ "$LOOP_DEV" = "-userdata" ]; then
	mkfs.ext3 $BLOCK_DIR/loop$LOOP_DEV
else
	if [ "$LOOP_DEV" = "-userdata" ]; then
		mke2fs $DISABLE_JOURNAL -T $USERDATA_FSTYPE $BLOCK_DIR/loop$LOOP_DEV
	fi
fi
