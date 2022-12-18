using Shared;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class WeaponMagazineController : MonoBehaviour
{
    [SerializeField]
    private WeaponController weaponController = null;

    private PlayerManager playerManager = null;

    // Start is called before the first frame update
    void Start()
    {
        if (PlayerManager.Instance)
        {
            playerManager = PlayerManager.Instance;
        }
    }

    private void OnTriggerEnter(Collider other)
    {
        PropController propController = other.GetComponent<PropController>();
        if (propController && (propController.PropStateType == PropStateType.Grabbed))
        {

            if (other.CompareTag(Consts.magazine))
            {

                PistolMagazine pistolMagazine = other.GetComponent<PistolMagazine>();
                if (pistolMagazine) {
                    weaponController.Reload(pistolMagazine);
                }
                GameObject.Destroy(other.gameObject);
            }
        }
    }
}
