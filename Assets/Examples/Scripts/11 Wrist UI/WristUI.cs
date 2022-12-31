using UnityEngine;
using UnityEngine.InputSystem;
public class WristUI : MonoBehaviour
{
    [SerializeField]
    private InputActionAsset inputActions = null;

    private Canvas _wristUICanvas = null;
    private InputAction _menu = null;


    private void Start()
    {
        _wristUICanvas = GetComponent<Canvas>();
        _menu = inputActions.FindActionMap("XRI LeftHand").FindAction("Menu");
        _menu.Enable();
        _menu.performed += ToggleMenu;
    }

    private void OnDestroy()
    {
        _menu.performed -= ToggleMenu;
    }

    public void ToggleMenu(InputAction.CallbackContext context)
    {
        _wristUICanvas.enabled = !_wristUICanvas.enabled;
    }
}
