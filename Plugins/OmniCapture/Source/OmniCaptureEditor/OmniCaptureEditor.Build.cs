using UnrealBuildTool;

public class OmniCaptureEditor : ModuleRules
{
    public OmniCaptureEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "Slate",
            "SlateCore",
            "UMG",
            "OmniCapture"  // ��ģ������
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "InputCore",
            "EditorStyle",
            "LevelEditor",         // �༭�����Ĺ���
            "Projects",
            "PropertyEditor",      // �����Զ������Ա༭
            "ToolMenus",           // ���ڴ����Զ��幤����
            "UnrealEd",            // �༭�����Ĺ���
            "DesktopPlatform"      // �������Ŀ¼ѡ��Ի���
        });
    }
}
