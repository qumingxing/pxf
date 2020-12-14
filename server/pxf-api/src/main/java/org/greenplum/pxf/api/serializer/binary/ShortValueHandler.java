package org.greenplum.pxf.api.serializer.binary;

import java.io.DataOutput;
import java.io.IOException;

public class ShortValueHandler<T extends Number> extends BaseBinaryValueHandler<T> {

    @Override
    protected void internalHandle(DataOutput buffer, final T value) throws IOException {
        buffer.writeInt(2);
        buffer.writeShort(value.shortValue());
    }
}
