# Breaking Changes

Breaking Changes by version they were introduced in.

## v0.5.0

The object script `initDelete` function got split into `init` and `destroy`.\
Newly created scripts use the newer version, old scripts will fail on building the project.

To migrate existing scripts, split the existing function. For example:
```cpp
void initDelete(Object& obj, Data *data, bool isDelete)
{
  if(isDelete) {
    rspq_call_deferred((void(*)(void*))rspq_block_free, data->dplBg);
    return;
  }

  rspq_block_begin();
  ...
  data->dplBg = rspq_block_end();
}
```
Becomes:
```cpp
void init(Object& obj, Data *data)
{
  rspq_block_begin();
  ...
  data->dplBg = rspq_block_end();
}

void destroy(Object& obj, Data *data)
{
  rspq_call_deferred((void(*)(void*))rspq_block_free, data->dplBg);
}
```
