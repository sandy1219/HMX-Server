//------------------------------------------------------------------------------
// <auto-generated>
//     This code was generated by a tool.
//
//     Changes to this file may cause incorrect behavior and will be lost if
//     the code is regenerated.
// </auto-generated>
//------------------------------------------------------------------------------

// Generated from: Cmd.proto
namespace Cmd
{
  [global::System.Serializable, global::ProtoBuf.ProtoContract(Name=@"CProtoCmdRequestLogin")]
  public partial class CProtoCmdRequestLogin : global::ProtoBuf.IExtensible
  {
    public CProtoCmdRequestLogin() {}
    
    private long _pid;
    [global::ProtoBuf.ProtoMember(1, IsRequired = true, Name=@"pid", DataFormat = global::ProtoBuf.DataFormat.TwosComplement)]
    public long pid
    {
      get { return _pid; }
      set { _pid = value; }
    }
    private string _key;
    [global::ProtoBuf.ProtoMember(2, IsRequired = true, Name=@"key", DataFormat = global::ProtoBuf.DataFormat.Default)]
    public string key
    {
      get { return _key; }
      set { _key = value; }
    }
    private int _account_id;
    [global::ProtoBuf.ProtoMember(3, IsRequired = true, Name=@"account_id", DataFormat = global::ProtoBuf.DataFormat.TwosComplement)]
    public int account_id
    {
      get { return _account_id; }
      set { _account_id = value; }
    }
    private global::ProtoBuf.IExtension extensionObject;
    global::ProtoBuf.IExtension global::ProtoBuf.IExtensible.GetExtensionObject(bool createIfMissing)
      { return global::ProtoBuf.Extensible.GetExtensionObject(ref extensionObject, createIfMissing); }
  }
  
}